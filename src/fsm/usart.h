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

/* This handles the USART 3 to the GPS receiver, and fills a queue of
 * NMEA messages.
 *
 * It also handles the debug USART 1 and allows sending messages to the PC.
 */

#ifndef __USART_H_
#define __USART_H_

#define MAX_NMEA_SENTENCE_LEN 256

// Initialise both USART1 for PC and USART3 for GPS
void usart_init(void);

// Send the str to the GPS receiver
void usart_gps_puts(const char* str);

// a printf to send data to the PC
void usart_debug(const char *format, ...);

// Send a string to the PC
void usart_debug_puts(const char* str);

// Get a MAX_NMEA_SENTENCE_LEN sized NMEA sentence
// Return 1 on success
int usart_get_nmea_sentence(char* nmea);

#endif //__USART_H_

