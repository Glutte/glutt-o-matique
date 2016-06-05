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

/* This handles the USART 3 to the GPS receiver, and fills a queue of
 * NMEA messages.
 *
 * It also handles the debug USART 2 and allows sending messages to the PC.
 */

#ifndef __USART_H_
#define __USART_H_

#ifdef STM32F4XX
#  include <stm32f4xx_usart.h>
#else
#  define USART_TypeDef int
#  define USART2 ((USART_TypeDef*)2)
#  define USART3 ((USART_TypeDef*)3)
#endif

#define MAX_NMEA_SENTENCE_LEN 256

// Initialise USART2 for PC debugging
void usart_init(void);

// Initialise USART3 for GPS and NMEA queue
// Needs running scheduler
void usart_gps_init(void);

// Send the str to the GPS receiver
void usart_gps_puts(const char* str);

// a printf to send data to the PC
void usart_debug(const char *format, ...);

// Send a string to the PC
void usart_debug_puts(const char* str);
void usart_debug_puts_header(const char* hdr, const char* str);

// Get a MAX_NMEA_SENTENCE_LEN sized NMEA sentence
// Return 1 on success
int usart_get_nmea_sentence(char* nmea);

void usart_debug_timestamp(void);

void usart_gps_specific_init(void);

void usart_process_char(char);
void usart_gps_process_char(char);

void usart_puts(USART_TypeDef*, const char*);

#endif //__USART_H_

