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

/* A set of common routines for internal timekeeping and a LFSR to generate
 * a random number
 */

#include <stdint.h>

#ifndef _COMMON_H_
#define _COMMON_H_

void common_init(void);

// Return the current timestamp in milliseconds. Timestamps are monotonic, and not
// wall clock time.
uint64_t timestamp_now(void);

/* Convert date to day of week.
 * day:   1-31
 * month: 1-12
 * year:  1900 to 3000
 *
 * Returns 1=Monday, ... 6=Saturday, 7=Sunday
 */
int dayofweek(uint8_t day, uint8_t month, uint16_t year);

// Return either 0 or 1, somewhat randomly
int random_bool(void);

// Fault handling mechanism
#define FAULT_SOURCE_MAIN  1
#define FAULT_SOURCE_GPS   2
#define FAULT_SOURCE_I2C   3
#define FAULT_SOURCE_USART 4
void trigger_fault(int source);

#endif // _COMMON_H_

