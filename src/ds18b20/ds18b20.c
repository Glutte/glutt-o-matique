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

#include "ds18b20.h"
#include "tm_stm32f4_ds18b20.h"
#include "tm_stm32f4_onewire.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"

static TM_OneWire_t tm_onewire;

void ds18b20_init()
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = ONEWIRE_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    TM_OneWire_Init(&tm_onewire, GPIOA, ONEWIRE_PIN);
}

int ds18b20_gettemp(float *temperature)
{
    uint8_t rom_addr[8];

    TM_DS18B20_StartAll(&tm_onewire);
    int status = TM_OneWire_First(&tm_onewire);
    if (status) {
        //Save ROM number from device
        TM_OneWire_GetFullROM(&tm_onewire, rom_addr);

        TM_DS18B20_Start(&tm_onewire, rom_addr);

        // The sensor needs time to do the conversion
        vTaskDelay(100 / portTICK_RATE_MS);

        status = TM_DS18B20_Read(&tm_onewire, rom_addr, temperature);
    }

    return status;
}
