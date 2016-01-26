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

#ifndef _PIO_H_
#define _PIO_H_

#include <stddef.h>
#include <stdint.h>
#include "fsm.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"

/* See pio.txt for PIO allocation details */

/* On GPIO C */
#define GPIO_PIN_QRP_n    GPIO_Pin_1
#define GPIO_PIN_TX       GPIO_Pin_2
#define GPIO_PIN_1750_n   GPIO_Pin_4
#define GPIO_PIN_MOD_OFF  GPIO_Pin_5
#define GPIO_PIN_SQ_n     GPIO_Pin_6
#define GPIO_PIN_U        GPIO_Pin_8
#define GPIO_PIN_QRP_out  GPIO_Pin_9
#define GPIO_PIN_D        GPIO_Pin_11
#define GPIO_PIN_REPLIE_n GPIO_Pin_13
#define GPIO_PIN_FAX_n    GPIO_Pin_14


#define GPIOC_OUTPUT_PINS ( \
                           GPIO_PIN_TX | \
                           GPIO_PIN_MOD_OFF | \
                           GPIO_PIN_QRP_out | \
                           0)

#undef GPIOC_OPENDRAIN_PINS
#undef GPIOC_INPUT_PU_PINS

#define GPIOC_INPUT_PINS ( \
                          GPIO_PIN_QRP_n | \
                          GPIO_PIN_1750_n | \
                          GPIO_PIN_SQ_n | \
                          GPIO_PIN_U | \
                          GPIO_PIN_D | \
                          GPIO_PIN_REPLIE_n | \
                          0 )

/* Analog inputs */
// TODO: SWR forward power
// TODO: SWR reflected power

void pio_init(void);

void pio_set_tx(int on);
void pio_set_mod_off(int mod_off);
void pio_set_qrp(int on);

void pio_set_fsm_signals(struct fsm_input_signals_t* sig);

#endif // _PIO_H_

