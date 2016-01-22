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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "common.h"
#include "usart.h"
#include <stm32f4xx.h>
#include <stm32f4xx_usart.h>
#include <stm32f4xx_conf.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* USART 3 on PD8 and PD9
 * See pio.txt for PIO allocation details */
const uint16_t GPIOD_PIN_USART3_TX = GPIO_Pin_8;
const uint16_t GPIOD_PIN_USART3_RX = GPIO_Pin_9;

/* USART 3 on PA9 and PA10 */
const uint16_t GPIOA_PIN_USART1_TX = GPIO_Pin_10;
const uint16_t GPIOA_PIN_USART1_RX = GPIO_Pin_9;

// The ISR writes into this buffer
static char nmea_sentence[MAX_NMEA_SENTENCE_LEN];
static int  nmea_sentence_last_written = 0;

// Once a completed NMEA sentence is received in the ISR,
// it is appended to this queue
static QueueHandle_t usart_nmea_queue;

void usart_init()
{
    // ============== PC DEBUG USART ===========
    RCC_APB1PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = GPIOA_PIN_USART1_RX | GPIOA_PIN_USART1_TX;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    // Setup USART1 for 9600,8,N,1
    USART_InitTypeDef USART_InitStruct;
    USART_InitStruct.USART_BaudRate = 9600;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStruct);

#if 0
    // enable the USART1 receive interrupt
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_SetPriority(USART1_IRQn, 6);

    // finally this enables the complete USART1 peripheral
    USART_Cmd(USART1, ENABLE);
#endif
}

void usart_gps_init()
{
    usart_nmea_queue = xQueueCreate(15, MAX_NMEA_SENTENCE_LEN);
    if (usart_nmea_queue == 0) {
        while(1); /* fatal error */
    }

    return;

    // ============== GPS USART ===========
    // Setup GPIO D and connect to USART 3
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = GPIOD_PIN_USART3_RX | GPIOD_PIN_USART3_TX;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3);

    // Setup USART3 for 9600,8,N,1
    USART_InitTypeDef USART_InitStruct;
    USART_InitStruct.USART_BaudRate = 9600;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART3, &USART_InitStruct);


    // enable the USART3 receive interrupt
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_SetPriority(USART3_IRQn, 6);

    // finally this enables the complete USART3 peripheral
    USART_Cmd(USART3, ENABLE);
}

static void usart_puts(USART_TypeDef* USART, const char* str)
{
    while(*str) {
        // wait until data register is empty
        while ( !(USART->SR & 0x00000040) );
        USART_SendData(USART, *str);
        str++;
    }
}

void usart_gps_puts(const char* str)
{
    return usart_puts(USART3, str);
}

#define MAX_MSG_LEN 80
static char usart_debug_message[MAX_MSG_LEN];

void usart_debug(const char *format, ...)
{
    va_list list;
    va_start(list, format);
    vsnprintf(usart_debug_message, MAX_MSG_LEN-1, format, list);
    usart_puts(USART1, usart_debug_message);
    va_end(list);
}

void usart_debug_puts(const char* str)
{
    usart_puts(USART1, str);
}

int usart_get_nmea_sentence(char* nmea)
{
    return xQueueReceive(usart_nmea_queue, nmea, portMAX_DELAY);
}


static void usart_clear_nmea_buffer(void)
{
    for (int i = 0; i < MAX_NMEA_SENTENCE_LEN; i++) {
        nmea_sentence[i] = '\0';
    }
    nmea_sentence_last_written = 0;
}

void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(USART3, USART_IT_RXNE)) {
        char t = USART3->DR;

        if (nmea_sentence_last_written == 0) {
            if (t == '$') {
                // Likely new start of sentence
                nmea_sentence[nmea_sentence_last_written] = t;
                nmea_sentence_last_written++;
            }
        }
        else if (nmea_sentence_last_written < MAX_NMEA_SENTENCE_LEN) {
            nmea_sentence[nmea_sentence_last_written] = t;
            nmea_sentence_last_written++;

            if (t == '\n') {
                int success = xQueueSendToBackFromISR(
                        usart_nmea_queue,
                        nmea_sentence,
                        NULL);

                if (success == pdFALSE) {
                    trigger_fault(FAULT_SOURCE_USART);
                }

                usart_clear_nmea_buffer();
            }
        }
        else {
            // Buffer overrun without a meaningful NMEA message.
            usart_clear_nmea_buffer();
        }
    }
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE)) {
        char t = USART1->DR;
        (void)t; // ignore t
    }
}

