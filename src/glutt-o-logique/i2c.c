/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Matthias P. Braendli
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#include "GPIO/i2c.h"
#include "Core/common.h"
#include "Audio/audio.h"

#include "stm32f4xx_conf.h"
#include "stm32f4xx_i2c.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
#include "GPIO/usart.h"

/* I2C 1 on PB9 and PB6
 * See pio.txt for PIO allocation details */
const uint16_t GPIOB_PIN_SDA = GPIO_Pin_9;
const uint16_t GPIOB_PIN_SCL = GPIO_Pin_6;


static int i2c_init_done = 0;
static int i2c_error = 0;

static I2C_TypeDef* const I2Cx = I2C1;

static SemaphoreHandle_t i2c_semaphore;

static void i2c_device_init(void);

/* According to I2C spec UM10204:
 * 3.1.16 Bus clear
 * In the unlikely event where the clock (SCL) is stuck LOW, the preferential
 * procedure is to reset the bus using the HW reset signal if your I2C devices
 * have HW reset inputs. If the I2C devices do not have HW reset inputs, cycle
 * power to the devices to activate the mandatory internal Power-On Reset (POR)
 * circuit.
 *
 * If the data line (SDA) is stuck LOW, the master should send nine clock
 * pulses. The device that held the bus LOW should release it sometime within
 * those nine clocks. If not, then use the HW reset or cycle power to clear the
 * bus.
 *
 * The only device on the i2c bus is the audio codec.
 */
static int i2c_recover_count;
static void i2c_recover_from_lockup(void)
{
    usart_debug_puts("ERROR: I2C lockup\r\n");

    i2c_recover_count++;
    if (i2c_recover_count > 3) {
        trigger_fault(FAULT_SOURCE_I2C);
    }

    I2C_SoftwareResetCmd(I2Cx, ENABLE);
    audio_put_codec_in_reset();
    for (int i = 0; i < 0x4fff; i++) {
        __asm__ volatile("nop");
    }
    I2C_SoftwareResetCmd(I2Cx, DISABLE);
    audio_reinit_codec();
}

static void i2c_device_init(void)
{
    // Configure I2C SCL and SDA pins.
    GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIOB_PIN_SCL | GPIOB_PIN_SDA;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_I2C1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_I2C1);

    // Reset I2C.
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, DISABLE);

    // configure I2C1
    I2C_InitTypeDef I2C_InitStruct;
    I2C_InitStruct.I2C_ClockSpeed = 100000; //Hz
    I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStruct.I2C_DutyCycle = I2C_DutyCycle_2; // 50% duty cycle
    I2C_InitStruct.I2C_OwnAddress1 = 0x00;          // not relevant in master mode

    // disable acknowledge when reading (can be changed later on)
    I2C_InitStruct.I2C_Ack = I2C_Ack_Disable;

    I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C1, &I2C_InitStruct);

    // enable I2C1
    I2C_Cmd(I2C1, ENABLE);

    i2c_recover_count = 0;
}

void i2c_init()
{
    if (i2c_init_done == 1) {
        return;
    }

    i2c_semaphore = xSemaphoreCreateBinary();

    if ( i2c_semaphore == NULL ) {
        trigger_fault(FAULT_SOURCE_I2C);
    }
    else {
        xSemaphoreGive(i2c_semaphore);
    }

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    i2c_device_init();
    i2c_init_done = 1;
}

static int i2c_check_busy_flag(void)
{
    const TickType_t i2c_timeout = pdMS_TO_TICKS(1000);
    const TickType_t time_start = xTaskGetTickCount();

    while (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY)) {
        const TickType_t time_now = xTaskGetTickCount();

        if (time_now - time_start > i2c_timeout) {
            i2c_error = 1;
            return 0;
        }
    }

    return 1;
}

static int i2c_check_event(uint32_t event)
{
    const TickType_t i2c_timeout = pdMS_TO_TICKS(1000);
    const TickType_t time_start = xTaskGetTickCount();

    while (!I2C_CheckEvent(I2Cx, event)) {
        const TickType_t time_now = xTaskGetTickCount();

        if (time_now - time_start > i2c_timeout) {
            i2c_error = 1;
            return 0;
        }
    }

    return 1;
}

static int i2c_start(uint8_t device, uint8_t direction)
{
    I2C_GenerateSTART(I2Cx, ENABLE);

    // wait for bus free
    if (!i2c_check_event(I2C_EVENT_MASTER_MODE_SELECT)) {
        I2C_GenerateSTART(I2Cx, DISABLE);
        return 0;
    }

    I2C_Send7bitAddress(I2Cx, device << 1, direction);

    /* wait for I2C1 EV6, check if
     * either Slave has acknowledged Master transmitter or
     * Master receiver mode, depending on the transmission
     * direction
     */
    uint32_t event = 0;
    if (direction == I2C_Direction_Transmitter) {
        event = I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED;
    }
    else if (direction == I2C_Direction_Receiver) {
        event = I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED;
    }
    else {
        trigger_fault(FAULT_SOURCE_I2C);
    }

    if (!i2c_check_event(event)) {
        return 0;
    }

    return 1;
}

static int i2c_send(uint8_t data)
{
    I2C_SendData(I2Cx, data);

    // wait for I2C1 EV8_2 --> byte has been transmitted
    return i2c_check_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED);
}

int i2c_write(uint8_t device, const uint8_t *txbuf, int len)
{
    if (i2c_init_done == 0) {
        trigger_fault(FAULT_SOURCE_I2C);
    }

    int success = i2c_check_busy_flag();

    if (success) {
        success = i2c_start(device, I2C_Direction_Transmitter);
    }

    if (success) {
        for (int i = 0; i < len; i++) {
            success = i2c_send(txbuf[i]);
            if (!success) {
                break;
            }
        }

        I2C_GenerateSTOP(I2Cx, ENABLE);
        success = i2c_check_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED);
    }

    return success;
}

static int i2c_read_nobuscheck(uint8_t device, uint8_t *rxbuf, int len)
{
    if (i2c_init_done == 0) {
        trigger_fault(FAULT_SOURCE_I2C);
    }

    if (i2c_start(device, I2C_Direction_Receiver)) {
        for (int i = 0; i < len; i++) {
            if (i == len-1) {
                I2C_AcknowledgeConfig(I2Cx, DISABLE);
            }
            else {
                I2C_AcknowledgeConfig(I2Cx, ENABLE);
            }

            // wait until one byte has been received, possibly timout
            if (!i2c_check_event(I2C_EVENT_MASTER_BYTE_RECEIVED)) {
                I2C_GenerateSTOP(I2Cx, ENABLE);
                return 0;
            }

            if (i == len-1) {
                I2C_GenerateSTOP(I2Cx, ENABLE);
            }

            rxbuf[i] = I2C_ReceiveData(I2Cx);
        }
        return len;
    }

    return 0;
}

int i2c_read(uint8_t device, uint8_t *rxbuf, int len)
{
    int success = i2c_check_busy_flag();

    if (success) {
        success = i2c_read_nobuscheck(device, rxbuf, len);
    }

    return success;
}

int i2c_read_from(uint8_t device, uint8_t address, uint8_t *rxbuf, int len)
{
    if (i2c_init_done == 0) {
        trigger_fault(FAULT_SOURCE_I2C);
    }

    int success = i2c_check_busy_flag();

    if (success) {
        success = i2c_start(device, I2C_Direction_Transmitter);
    }

    if (success) {
        success = i2c_send(address);
    }
    // Don't do a STOP

    if (success) {
        success = i2c_read_nobuscheck(device, rxbuf, len);
    }

    return success;
}



void i2c_transaction_start()
{
    if (i2c_init_done == 0) {
        trigger_fault(FAULT_SOURCE_I2C);
    }
    xSemaphoreTake(i2c_semaphore, portMAX_DELAY);
}

void i2c_transaction_end()
{
    if (i2c_init_done == 0) {
        trigger_fault(FAULT_SOURCE_I2C);
    }

    if ( i2c_error ) {
        i2c_recover_from_lockup();

        i2c_error = 0;
    }

    xSemaphoreGive(i2c_semaphore);
}

