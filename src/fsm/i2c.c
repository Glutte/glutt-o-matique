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
#include "stm32f4xx_conf.h"
#include "stm32f4xx_i2c.h"
#include "stm32f4xx.h"

static int i2c_init_done = 0;

static I2C_TypeDef* const I2Cx = I2C1;

void i2c_init()
{
    if (i2c_init_done == 1) {
        return;
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

    i2c_init_done = 1;
}


static void i2c_start(uint8_t device, uint8_t direction)
{
    while(I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY));
    I2C_GenerateSTART(I2Cx, ENABLE);
    // wait for I2C1 EV5 --> Slave has acknowledged start condition
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT));

    // Send slave Address for write
    I2C_Send7bitAddress(I2Cx, device << 1, direction);

    /* wait for I2C1 EV6, check if
     * either Slave has acknowledged Master transmitter or
     * Master receiver mode, depending on the transmission
     * direction
     */
    if(direction == I2C_Direction_Transmitter) {
        while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
    }
    else if(direction == I2C_Direction_Receiver) {
        while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));
    }
}

static void i2c_send(uint8_t data) {
    I2C_SendData(I2Cx, data);
    // wait for I2C1 EV8_2 --> byte has been transmitted
    while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
}

void i2c_write(uint8_t device, const uint8_t *txbuf, int len) {
    i2c_start(device, I2C_Direction_Transmitter);
    for (int i = 0; i < len; i++) {
        i2c_send(txbuf[i]);
    }
    I2C_GenerateSTOP(I2Cx, ENABLE);
}

int i2c_read(uint8_t device, uint8_t *rxbuf, int len)
{
    i2c_start(device, I2C_Direction_Receiver);
    for (int i = 0; i < len; i++) {
        if (i < len-1) {
            I2C_AcknowledgeConfig(I2Cx, ENABLE);
            // wait until one byte has been received
            while( !I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED) );
            rxbuf[i] = I2C_ReceiveData(I2Cx);
        }
        else {
            I2C_AcknowledgeConfig(I2Cx, DISABLE);
            I2C_GenerateSTOP(I2Cx, ENABLE);
            // wait until one byte has been received
            while( !I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED) );
            rxbuf[i] = I2C_ReceiveData(I2Cx);
        }
    }

    return len;
}

