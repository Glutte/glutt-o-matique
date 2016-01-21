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

#include "pio.h"
#include "common.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

void read_fsm_input_task(void *pvParameters);

struct fsm_input_signals_t pio_signals;

void pio_init()
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // Init pio
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Pin   = GPIOC_OUTPUT_PINS;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

#if GPIOC_OPENDRAIN_PINS
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Pin   = GPIOC_OPENDRAIN_PINS;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
#endif

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Pin   = GPIOC_INPUT_PU_PINS;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Pin   = GPIOC_INPUT_PINS;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    xTaskCreate(
            read_fsm_input_task,
            "TaskPIO",
            configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            NULL);
}

void pio_set_fsm_signals(struct fsm_input_signals_t* sig)
{
    *sig = pio_signals;
}

void read_fsm_input_task(void *pvParameters)
{
    while (1) {
        pio_signals.qrp       = GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_QRP_n) ? 0 : 1;
#if INVERTED_1750
        pio_signals.tone_1750 = GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_1750) ? 0 : 1;
#else
        pio_signals.tone_1750 = GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_1750) ? 1 : 0;
#endif
        pio_signals.carrier   = GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_RX_n) ? 0 : 1;
        pio_signals.discrim_u = GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_U_n) ? 0 : 1;
        pio_signals.discrim_d = GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_D_n) ? 0 : 1;

        vTaskDelay(100 / portTICK_RATE_MS);
    }
}

void pio_set_tx(int on)
{
    // Led is active high, TX_n is inverted logic
    if (on) {
        GPIO_SetBits(GPIOC, GPIO_PIN_LED_red | GPIO_PIN_TX);
    }
    else {
        GPIO_ResetBits(GPIOC, GPIO_PIN_LED_red | GPIO_PIN_TX);
    }
}

void pio_set_led_grn(int on)
{
    if (on) {
        GPIO_SetBits(GPIOC, GPIO_PIN_LED_grn);
    }
    else {
        GPIO_ResetBits(GPIOC, GPIO_PIN_LED_grn);
    }
}

void pio_set_led_yel(int on)
{
    if (on) {
        GPIO_SetBits(GPIOC, GPIO_PIN_LED_yel);
    }
    else {
        GPIO_ResetBits(GPIOC, GPIO_PIN_LED_yel);
    }
}
