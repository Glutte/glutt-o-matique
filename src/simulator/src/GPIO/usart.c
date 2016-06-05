/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Maximilien Cuony
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


#include "../../../common/includes/GPIO/usart.h"

extern char uart_recv_txt[4096];
int uart_recv_pointer = 0;


void usart_move_buffer_up();


void usart_init() {

    // Zero buffer
    for (int i = 0; i < 4096; i++) {
        uart_recv_txt[i] = '\0';
    }

}

void usart_gps_specific_init() {
}

void usart_move_buffer_up() {

    for (int i = 0; i <= 4010; i++) {
        uart_recv_txt[i] = uart_recv_txt[i + 10];
        uart_recv_txt[i + 1] = '\0';
    }

    uart_recv_pointer -= 10;

}

// Make sure Tasks are suspended when this is called!
void usart_puts(USART_TypeDef* USART, const char* str) {

    if (USART == USART2) {
        while(*str) {
            if (*str != '\r') {
                uart_recv_txt[uart_recv_pointer+1] = '\0';
                uart_recv_txt[uart_recv_pointer] = *str;
                uart_recv_pointer++;

                if (uart_recv_pointer >= 4000) {
                    usart_move_buffer_up();
                }
            }
            str++;
        }
    }
}

void gui_usart_send(char * string) {

    while(*string) {
        usart_process_char(*string);

        string++;
    }

}
