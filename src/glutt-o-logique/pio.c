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
#define GPIO_PIN_FAX      GPIO_Pin_14


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
                          GPIO_PIN_FAX | \
                          0 )

#include "GPIO/pio.h"
#include "Core/common.h"
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

#if defined(GPIOC_OPENDRAIN_PINS)
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Pin   = GPIOC_OPENDRAIN_PINS;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
#endif

#if defined(GPIOC_INPUT_PU_PINS)
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Pin   = GPIOC_INPUT_PU_PINS;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
#endif

#if defined(GPIOC_INPUT_PINS)
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Pin   = GPIOC_INPUT_PINS;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
#endif

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

void read_fsm_input_task(void __attribute__ ((unused))*pvParameters)
{
    while (1) {
        pio_signals.qrp =
            GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_QRP_n) ? 0 : 1;

        pio_signals.tone_1750 =
            GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_1750_n) ? 0 : 1;

        pio_signals.sq =
            GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_SQ_n) ? 0 : 1;

        pio_signals.discrim_u =
            GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_U) ? 1 : 0;

        pio_signals.discrim_d =
            GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_D) ? 1 : 0;

        pio_signals.wind_generator_ok =
            GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_REPLIE_n) ? 1 : 0;

        pio_signals.sstv_mode =
            GPIO_ReadInputDataBit(GPIOC, GPIO_PIN_FAX) ? 1 : 0;

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void pio_set_tx(int on)
{
    if (on) {
        GPIO_SetBits(GPIOC, GPIO_PIN_TX);
    }
    else {
        GPIO_ResetBits(GPIOC, GPIO_PIN_TX);
    }
}

void pio_set_qrp(int on)
{
    if (on) {
        GPIO_SetBits(GPIOC, GPIO_PIN_QRP_out);
    }
    else {
        GPIO_ResetBits(GPIOC, GPIO_PIN_QRP_out);
    }
}

void pio_set_mod_off(int mod_off)
{
    if (mod_off) {
        GPIO_SetBits(GPIOC, GPIO_PIN_MOD_OFF);
    }
    else {
        GPIO_ResetBits(GPIOC, GPIO_PIN_MOD_OFF);
    }
}

int pio_read_button() {
    return GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0) == Bit_SET;
}
