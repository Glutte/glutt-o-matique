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

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "Core/common.h"
#include "GPIO/temperature.h"


float _temperature_last_value;
int _temperature_valid_since;

#define TEMP_VALID_DURATION (60*15) // 15 minutes


void temperature_task(void *pvParameters);


void temperature_init() {

    xTaskCreate(
        temperature_task,
        "TaskTemperature",
        4*configMINIMAL_STACK_SIZE,
        (void*) NULL,
        tskIDLE_PRIORITY + 2UL,
        NULL);
}

int temperature_valid() {
    const uint64_t now = timestamp_now();
    return _temperature_valid_since > 0 && _temperature_valid_since + TEMP_VALID_DURATION < now;
}

// Return 1 if the the current temperature is valid, and write it into temp
int temperature_get(float *temp) {
    if (temperature_valid()) {
        *temp = _temperature_last_value;
        return 1;
    }
    return 0;
}

