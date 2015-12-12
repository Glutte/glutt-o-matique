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

#include "common.h"
#include "FreeRTOS.h"
#include "timers.h"

static uint64_t common_timestamp = 0; // milliseconds since startup
static TimerHandle_t common_timer;

// The LFSR is used as random number generator
static const uint16_t lfsr_start_state = 0x12ABu;
static uint16_t lfsr;

static void common_increase_timestamp(TimerHandle_t t);

void common_init(void)
{
    common_timer = xTimerCreate("Timer",
            portTICK_PERIOD_MS,
            pdTRUE, // Auto-reload
            NULL,   // No unique id
            common_increase_timestamp
            );

    xTimerStart(common_timer, 0);

    lfsr = lfsr_start_state;
}

static void common_increase_timestamp(TimerHandle_t t)
{
    common_timestamp++;
}

uint64_t timestamp_now(void)
{
    return common_timestamp;
}

// Return either 0 or 1, somewhat randomly
int random_bool(void)
{
    uint16_t bit;

    /* taps: 16 14 13 11; feedback polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    lfsr =  (lfsr >> 1) | (bit << 15);

    return bit;
}


