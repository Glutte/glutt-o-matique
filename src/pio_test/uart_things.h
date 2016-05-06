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

#pragma once

const uint16_t GPIOA_PIN_USART2_RX = GPIO_Pin_3;
const uint16_t GPIOA_PIN_USART2_TX = GPIO_Pin_2;


char msg[32];
static int cnt = 0;
void printcnt(void)
{
    snprintf(msg, 31, "%d ", cnt++);

    char* c = msg;
    while (*c) {
        USART_SendData(USART2, *c);
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) ;
        c++;
    }
}

static void usart_puts(USART_TypeDef* USART, const char* str)
{
    while(*str) {
        // wait until data register is empty
        USART_SendData(USART, *str);
        while(USART_GetFlagStatus(USART, USART_FLAG_TXE) == RESET) ;
        str++;
    }
}
#define MAX_MSG_LEN 80
static char usart_debug_message[MAX_MSG_LEN];

void usart_debug(const char *format, ...)
{
    va_list list;
    va_start(list, format);
    vsnprintf(usart_debug_message, MAX_MSG_LEN-1, format, list);
    usart_puts(USART2, usart_debug_message);
    va_end(list);
}

void usart_debug_puts(const char* str)
{
    usart_puts(USART2, str);
}

void USART2_IRQHandler(void)
{
    /* RXNE handler */
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        /* If received 't', toggle LED and transmit 'T' */
        if((char)USART_ReceiveData(USART2) == 't')
        {
            USART_SendData(USART2, 'T');

            //while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) ;
        }
        else {
            USART_SendData(USART2, 'N');
            USART_SendData(USART2, 'o');
            USART_SendData(USART2, '!');
            USART_SendData(USART2, '\n');
        }
    }
}

