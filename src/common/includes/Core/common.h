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

/* A set of common routines for internal timekeeping and a LFSR to generate
 * a random number
 */

#pragma once

#include <stdint.h>
#include <time.h>

/* Feature defines */
#define ENABLE_PSK31 0

#define FLOAT_PI 3.1415926535897932384f

void common_init(void);

// Return the current timestamp in milliseconds. Timestamps are monotonic, and not
// wall clock time.
uint64_t timestamp_now(void);

// Calculate local time from GPS time, including daylight saving time
// Return 1 on success, 0 on failure
// A call to this function will invalidate the information inside 'time'
// regardless of return value.
int local_time(struct tm *time);

// Try to calculate local time, based on a past valid local time and the current timestamp
// Return 1 on success, 0 on failure
int local_derived_time(struct tm *time);

// Return either 0 or 1, somewhat randomly
int random_bool(void);

// Fault handling mechanism
#define FAULT_SOURCE_MAIN  1
#define FAULT_SOURCE_GPS 2
#define FAULT_SOURCE_I2C 3
#define FAULT_SOURCE_USART 4
#define FAULT_SOURCE_TASK_OVERFLOW 5
#define FAULT_SOURCE_CW_AUDIO_QUEUE 6
#define FAULT_SOURCE_ADC1 7
#define FAULT_SOURCE_TIM6_ISR 8
#define FAULT_SOURCE_ADC2_QUEUE 9
void trigger_fault(int source);

int find_last_sunday(const struct tm*);

#ifdef SIMULATOR
void __disable_irq(void);
#else
void hard_fault_handler_c(uint32_t *);
#endif

// Round a value to the nearest 0.5
float round_float_to_half_steps(float value);

#define GPS_MS_TIMEOUT 2000ul
