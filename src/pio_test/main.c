/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Matthias P. Braendli
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

#include "stm32f4xx.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stm32f4xx.h>
#include <stm32f4xx_usart.h>
#include <stm32f4xx_conf.h>
#include "delay.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "pio.h"


#include "uart_things.h"

void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                    signed char *pcTaskName )
{
    usart_debug("TASK OVERFLOW %s\r\n", pcTaskName);
    while (1) {};
}

void launcher_task(void *args);

int main(void)
{
    // ============== PC DEBUG USART ===========
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = GPIOA_PIN_USART2_RX | GPIOA_PIN_USART2_TX;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    // Setup USART2 for 9600,8,N,1
    USART_InitTypeDef USART_InitStruct;
    USART_InitStruct.USART_BaudRate = 9600;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &USART_InitStruct);

#if 0
    // enable the USART2 receive interrupt
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_SetPriority(USART2_IRQn, 6);
#endif

    // finally this enables the complete USART2 peripheral
    USART_Cmd(USART2, ENABLE);

    TaskHandle_t task_handle;
    xTaskCreate(
            launcher_task,
            "TaskLauncher",
            configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            &task_handle);

    /* Start the RTOS Scheduler */
    vTaskStartScheduler();

    /* HALT */
    while(1);
}

void launcher_task(void *args) {

    usart_debug_puts("pio init\r\n");

    pio_init();

    struct fsm_input_signals_t signals;
    struct fsm_input_signals_t previous_signals;

    pio_set_fsm_signals(&previous_signals);

    usart_debug(" qrp = %d ", previous_signals.qrp);
    usart_debug(" tone_1750 = %d ", previous_signals.tone_1750);
    usart_debug(" sq = %d ", previous_signals.sq);
    usart_debug(" discrim_u = %d ", previous_signals.discrim_u);
    usart_debug(" discrim_d = %d ", previous_signals.discrim_d);
    usart_debug(" wind_generator_ok = %d ", previous_signals.wind_generator_ok);
    usart_debug(" sstv_mode = %d\r\n", previous_signals.sstv_mode);

    while(1) {
        delay_ms(5 * 1000ul);

        pio_set_fsm_signals(&signals);

        if (previous_signals.qrp != signals.qrp) {
            usart_debug("pio qrp = %d\r\n", signals.qrp);
        }
        if (previous_signals.tone_1750 != signals.tone_1750) {
            usart_debug("pio tone_1750 = %d\r\n", signals.tone_1750);
        }
        if (previous_signals.sq != signals.sq) {
            usart_debug("pio sq = %d\r\n", signals.sq);
        }
        if (previous_signals.discrim_u != signals.discrim_u) {
            usart_debug("pio discrim_u = %d\r\n", signals.discrim_u);
        }
        if (previous_signals.discrim_d != signals.discrim_d) {
            usart_debug("pio discrim_d = %d\r\n", signals.discrim_d);
        }
        if (previous_signals.wind_generator_ok != signals.wind_generator_ok) {
            usart_debug("pio wind_generator_ok = %d\r\n", signals.wind_generator_ok);
        }
        if (previous_signals.sstv_mode != signals.sstv_mode) {
            usart_debug("pio sstv_mode = %d\r\n", signals.sstv_mode);
        }

        previous_signals = signals;

        pio_set_tx(signals.discrim_u);
        pio_set_qrp(signals.discrim_d);
        pio_set_mod_off(signals.tone_1750);
    }
}

