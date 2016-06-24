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

#pragma once

#include "stm32f4xx_conf.h"
#include "stm32f4xx_gpio.h"
#include "GPIO/analog.h"

#define PIN_SUPPLY GPIO_Pin_5
#define PIN_SWR_FWD GPIO_Pin_6
#define PIN_SWR_REFL GPIO_Pin_7

#define PINS_ANALOG (GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7)

#define ADC_CHANNEL_SUPPLY ADC_Channel_5
#define ADC_CHANNEL_SWR_FWD ADC_Channel_6
#define ADC_CHANNEL_SWR_REFL ADC_Channel_7

