/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Matthias P. Braendli
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
#include "stm32f4xx_conf.h"

/* See pio.txt for PIO allocation details */

#define GPIOA_PIN_DET_1750 GPIO_Pin_8
#define GPIOA_PIN_PUSHBTN  GPIO_Pin_0

#define GPIOC_PIN_SQ2      GPIO_Pin_0
#define GPIOC_PIN_QRP_n    GPIO_Pin_1
#define GPIOC_PIN_TX       GPIO_Pin_2
#define GPIOC_PIN_1750_n   GPIO_Pin_4
#define GPIOC_PIN_MOD_OFF  GPIO_Pin_5
#define GPIOC_PIN_SQ_n     GPIO_Pin_6
#define GPIOC_PIN_U        GPIO_Pin_8
#define GPIOC_PIN_QRP_out  GPIO_Pin_9
#define GPIOC_PIN_D        GPIO_Pin_11
#define GPIOC_PIN_REPLIE   GPIO_Pin_13
#define GPIOC_PIN_FAX      GPIO_Pin_14
#define GPIOC_PIN_GPS_EPPS GPIO_Pin_15


#define GPIOA_INPUT_PINS ( \
                           GPIOA_PIN_PUSHBTN | \
                           0)

#define GPIOA_OUTPUT_PINS ( \
                           GPIOA_PIN_DET_1750 | \
                           0)

#define GPIOC_OUTPUT_PINS ( \
                           GPIOC_PIN_TX | \
                           GPIOC_PIN_MOD_OFF | \
                           GPIOC_PIN_QRP_out | \
                           GPIOC_PIN_GPS_EPPS | \
                           GPIOC_PIN_FAX | \
                           GPIOC_PIN_SQ2 | \
                           0)

#undef GPIOC_OPENDRAIN_PINS
#undef GPIOC_INPUT_PU_PINS

#define GPIOC_INPUT_PINS ( \
                          GPIOC_PIN_QRP_n | \
                          GPIOC_PIN_1750_n | \
                          GPIOC_PIN_SQ_n | \
                          GPIOC_PIN_U | \
                          GPIOC_PIN_D | \
                          GPIOC_PIN_REPLIE | \
                          0 )

#define GPIOD_OUTPUT_PINS ( \
                           GPIOD_BOARD_LED_GREEN | \
                           GPIOD_BOARD_LED_ORANGE | \
                           GPIOD_BOARD_LED_RED | \
                           GPIOD_BOARD_LED_BLUE | \
                           0 )

#include "leds.h"

#include "GPIO/pio.h"
#include "Core/common.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

void read_fsm_input_task(void *pvParameters);

struct fsm_input_signals_t pio_signals;

static inline int all_equal(const uint8_t* v, uint8_t len)
{
    if (len < 2) {
        return 1;
    }

    for (uint8_t i = 1; i < len; i++) {
        if (v[i] != v[0])
            return 0;
    }
    return 1;
}

static inline int any_true(const uint8_t* v, uint8_t len)
{
    if (len < 2) {
        return 1;
    }

    for (uint8_t i = 0; i < len; i++) {
        if (v[i])
            return 1;
    }
    return 0;
}

/* Some signals need additional debouncing or delays, the following
 * variables are shift registers.
 */
#define DEBOUNCE_LEN_SQ 3
static uint8_t debounce_sq[DEBOUNCE_LEN_SQ] = {0};

/* D and U have to stay on longer than SQ, otherwise the
 * letter selection can fail, and transmit a K when it should
 * send a D or U
 */
#define DEBOUNCE_LEN_D_U 6
static uint8_t debounce_discrim_d[DEBOUNCE_LEN_D_U] = {0};
static uint8_t debounce_discrim_u[DEBOUNCE_LEN_D_U] = {0};

/* This comes from the front panel button */
#define DEBOUNCE_LEN_1750 3
static uint8_t debounce_1750[DEBOUNCE_LEN_1750] = {0};

void pio_init()
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // Init pio
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    // For the onboard peripherals, 4x LEDs, 1 push-button:
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    // GPIO A
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Pin =  GPIOA_INPUT_PINS;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Pin   = GPIOA_OUTPUT_PINS;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // GPIOC
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

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Pin   = GPIOC_INPUT_PINS;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // GPIO D
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Pin   = GPIOD_OUTPUT_PINS;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

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
            GPIO_ReadInputDataBit(GPIOC, GPIOC_PIN_QRP_n) ? 0 : 1;

        for (int i = 0; i < DEBOUNCE_LEN_SQ-1; i++) {
            debounce_sq[i] = debounce_sq[i+1];
        }

        debounce_sq[DEBOUNCE_LEN_SQ-1] =
            GPIO_ReadInputDataBit(GPIOC, GPIOC_PIN_SQ_n) ? 0 : 1;

        /* Only change the signal if its input has stabilised */
        if (all_equal(debounce_sq, DEBOUNCE_LEN_SQ)) {
            pio_signals.sq = debounce_sq[0];
        }

        /* The discrim U and D signals should be very sensitive:
         * if one toggles to 1, set to 1; reset to 0 only if all
         * are at 0.
         */
        for (int i = 0; i < DEBOUNCE_LEN_D_U-1; i++) {
            debounce_discrim_d[i] = debounce_discrim_d[i+1];
            debounce_discrim_u[i] = debounce_discrim_u[i+1];
        }

        debounce_discrim_u[DEBOUNCE_LEN_D_U-1] =
            GPIO_ReadInputDataBit(GPIOC, GPIOC_PIN_U) ? 1 : 0;
        pio_signals.discrim_u = any_true(debounce_discrim_u, DEBOUNCE_LEN_D_U);

        debounce_discrim_d[DEBOUNCE_LEN_D_U-1] =
            GPIO_ReadInputDataBit(GPIOC, GPIOC_PIN_D) ? 1 : 0;
        pio_signals.discrim_d = any_true(debounce_discrim_d, DEBOUNCE_LEN_D_U);


        for (int i = 0; i < DEBOUNCE_LEN_1750-1; i++) {
            debounce_1750[i] = debounce_1750[i+1];
        }

        debounce_1750[DEBOUNCE_LEN_1750-1] =
            GPIO_ReadInputDataBit(GPIOC, GPIOC_PIN_1750_n) ? 0 : 1;

        if (all_equal(debounce_1750, DEBOUNCE_LEN_1750)) {
            pio_signals.button_1750 = debounce_1750[0];
        }

        pio_signals.wind_generator_ok =
            GPIO_ReadInputDataBit(GPIOC, GPIOC_PIN_REPLIE) ? 0 : 1;

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void pio_set_tx(int on)
{
    if (on) {
        GPIO_SetBits(GPIOC, GPIOC_PIN_TX);
    }
    else {
        GPIO_ResetBits(GPIOC, GPIOC_PIN_TX);
    }
}

void pio_set_qrp(int on)
{
    if (on) {
        GPIO_SetBits(GPIOC, GPIOC_PIN_QRP_out);
    }
    else {
        GPIO_ResetBits(GPIOC, GPIOC_PIN_QRP_out);
    }
}

void pio_set_mod_off(int mod_off)
{
    if (mod_off) {
        GPIO_SetBits(GPIOC, GPIOC_PIN_MOD_OFF);
    }
    else {
        GPIO_ResetBits(GPIOC, GPIOC_PIN_MOD_OFF);
    }
}

int pio_read_button() {
    return GPIO_ReadInputDataBit(GPIOA,GPIOA_PIN_PUSHBTN) == Bit_SET;
}

void pio_set_gps_epps(int on)
{
    if (on) {
        GPIO_SetBits(GPIOC, GPIOC_PIN_GPS_EPPS);
    }
    else {
        GPIO_ResetBits(GPIOC, GPIOC_PIN_GPS_EPPS);
    }
}

void pio_set_fax(int on)
{
    if (on) {
        GPIO_SetBits(GPIOC, GPIOC_PIN_FAX);
    }
    else {
        GPIO_ResetBits(GPIOC, GPIOC_PIN_FAX);
    }
}

void pio_set_det_1750(int on)
{
    if (on) {
        GPIO_SetBits(GPIOA, GPIOA_PIN_DET_1750);
    }
    else {
        GPIO_ResetBits(GPIOA, GPIOA_PIN_DET_1750);
    }
}

void pio_set_sq2(int on)
{
    if (on) {
        GPIO_SetBits(GPIOC, GPIOC_PIN_SQ2);
    }
    else {
        GPIO_ResetBits(GPIOC, GPIOC_PIN_SQ2);
    }
}

