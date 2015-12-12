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

/*
Blue    in  QRP_n   PC1
Violet  out LED red PC2
Grey    in  1750    PC4
White   out LED yel PC5
Black   -   GND     GND
Brown   in  RX_n    PC6
Red     in  U_n     PC8
Orange  out LED grn PC9
Green   in  D_n     PC11
*/

#define GPIO_PIN_QRP_n   GPIO_Pin_1
#define GPIO_PIN_LED_red GPIO_Pin_2
#define GPIO_PIN_1750    GPIO_Pin_4
#define GPIO_PIN_LED_yel GPIO_Pin_5
#define GPIO_PIN_RX_n    GPIO_Pin_6
#define GPIO_PIN_U_n     GPIO_Pin_8
#define GPIO_PIN_LED_grn GPIO_Pin_9
#define GPIO_PIN_D_n     GPIO_Pin_11


#define PIO_OUTPUT_PINS GPIO_Pin_2 | GPIO_Pin_5 | GPIO_Pin_9

#define PIO_INPUT_PINS GPIO_Pin_1 | GPIO_Pin_4 | \
                       GPIO_Pin_6 | GPIO_Pin_8 | \
                       GPIO_Pin_11

void pio_init(void);

void pio_set_led_red(int on);
void pio_set_led_yel(int on);
void pio_set_led_grn(int on);

void pio_set_fsm_signals(struct fsm_input_signals_t* sig);

#endif // _PIO_H_

