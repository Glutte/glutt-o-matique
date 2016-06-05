/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Matthias P. Braendli, Maximilien Cuony
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

/* One-wire connection: PA1 */
#define TEMPERATURE_ONEWIRE_PIN GPIO_Pin_1

#include "stm32f4xx_conf.h"
#include "stm32f4xx.h"

#include "tm_stm32f4_ds18b20.h"
#include "tm_stm32f4_onewire.h"
#include "Core/delay.h"


#include "../common/src/GPIO/temperature.c"

const TickType_t _temperature_delay = 60000 / portTICK_PERIOD_MS; // 60s

static TM_OneWire_t tm_onewire;



void ds18b20_init() {
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = TEMPERATURE_ONEWIRE_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    vTaskSuspendAll();
    TM_OneWire_Init(&tm_onewire, GPIOA, TEMPERATURE_ONEWIRE_PIN);
    xTaskResumeAll();
}

int ds18b20_gettemp_one(float *temperature) {

    vTaskSuspendAll();

    uint8_t rom_addr[8];

    TM_DS18B20_StartAll(&tm_onewire);
    int status = TM_OneWire_First(&tm_onewire);
    if (status) {
        //Save ROM number from device
        TM_OneWire_GetFullROM(&tm_onewire, rom_addr);
        TM_DS18B20_Start(&tm_onewire, rom_addr);

        // The sensor needs time to do the conversion
        xTaskResumeAll();
        delay_ms(100);
        vTaskSuspendAll();
        status = TM_DS18B20_Read(&tm_onewire, rom_addr, temperature);
    }

    xTaskResumeAll();

    return status;
}

int ds18b20_gettemp(float *temperature) {
    int status;
    for (int i = 0; i < 10; i++) {
        status = ds18b20_gettemp_one(temperature);

        if (status) {
            break;
        }
        delay_ms(5);
    }
    return status;
}

static void temperature_task(void *pvParameters) {

    while (1) {

        if (!_temperature_valid) {
            ds18b20_init();
        }

        if (ds18b20_gettemp(&_temperature_last_value)) {
            _temperature_valid = 1;
        } else {
            _temperature_valid = 0;
        }

        vTaskDelay(_temperature_delay);

    }
}
