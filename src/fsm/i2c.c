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

#include "i2c.h"
#include "common.h"
#include "stm32f4xx_conf.h"
#include "stm32f4xx_i2c.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"

static int i2c_init_done = 0;
static int i2c_error = 0;

static I2C_TypeDef* const I2Cx = I2C1;

static SemaphoreHandle_t i2c_semaphore;

static void i2c_device_init(void)
{
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
    RCC_APB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    // Configure I2C SCL and SDA pins.
    GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_I2C1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_I2C1);

    // Reset I2C.
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, DISABLE);

    i2c_device_init();
    i2c_init_done = 1;
}


static int i2c_start(uint8_t device, uint8_t direction)
{
    I2C_GenerateSTART(I2Cx, ENABLE);

    // wait for I2C1 EV5 --> Slave has acknowledged start condition
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT));

    // Send slave Address for write
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

    while (!I2C_CheckEvent(I2Cx, event));

    return 1;
}

static int i2c_send(uint8_t data)
{
    I2C_SendData(I2Cx, data);

    // wait for I2C1 EV8_2 --> byte has been transmitted
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    return 1;
}

int i2c_write(uint8_t device, const uint8_t *txbuf, int len)
{
    if (i2c_init_done == 0) {
        trigger_fault(FAULT_SOURCE_I2C);
    }

    while (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY));

    int success = 0;

    if (i2c_start(device, I2C_Direction_Transmitter)) {
        for (int i = 0; i < len; i++) {
            success = i2c_send(txbuf[i]);
            if (!success) {
                break;
            }
        }
    }
    I2C_GenerateSTOP(I2Cx, ENABLE);
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    return success;
}

static int i2c_read_nobuscheck(uint8_t device, uint8_t *rxbuf, int len)
{
    if (i2c_init_done == 0) {
        trigger_fault(FAULT_SOURCE_I2C);
    }

    i2c_start(device, I2C_Direction_Receiver);
    for (int i = 0; i < len; i++) {
        if (i == len-1) {
            I2C_AcknowledgeConfig(I2Cx, DISABLE);
        }
        else {
            I2C_AcknowledgeConfig(I2Cx, ENABLE);
        }

        // wait until one byte has been received
        while( !I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED) );

        if (i == len-1) {
            I2C_GenerateSTOP(I2Cx, ENABLE);
        }

        rxbuf[i] = I2C_ReceiveData(I2Cx);
    }

    return len;
}

int i2c_read(uint8_t device, uint8_t *rxbuf, int len)
{
    while (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY));

    return i2c_read_nobuscheck(device, rxbuf, len);
}

int i2c_read_from(uint8_t device, uint8_t address, uint8_t *rxbuf, int len)
{
    if (i2c_init_done == 0) {
        trigger_fault(FAULT_SOURCE_I2C);
    }

    while (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY));

    int success = i2c_start(device, I2C_Direction_Transmitter);
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
        I2C_SoftwareResetCmd(I2Cx, ENABLE);
        I2C_SoftwareResetCmd(I2Cx, DISABLE);

        i2c_device_init();

        i2c_error = 0;
    }

    xSemaphoreGive(i2c_semaphore);
}

