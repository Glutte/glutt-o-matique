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

#include "Core/common.h"
#include "GPIO/usart.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "GPS/gps.h"
#include <time.h>
#include <math.h>

static uint64_t common_timestamp = 0; // milliseconds since startup
static TimerHandle_t common_timer;

// The LFSR is used as random number generator
static const uint16_t lfsr_start_state = 0x12ABu;
static uint16_t lfsr;

static void common_increase_timestamp(TimerHandle_t t);

struct tm last_derived_time;
static uint64_t last_derived_time_timestamp = 0;
int last_derived_time_valid = 0;
int last_derived_time_delta_applied = 0;


#ifdef SIMULATOR
long timestamp_delta = 0;
#endif

int find_last_sunday(const struct tm* time) {
    struct tm t = *time;

    // the last sunday can never be before the 20th
    t.tm_mday = 20;

    int last_sunday = 1;

    while (t.tm_mon == time->tm_mon) {
        t.tm_mday++;
        if (mktime(&t) == (time_t)-1) {
            return -1;
        }

        const int sunday = 0;
        if (t.tm_wday == sunday) {
            last_sunday = t.tm_mday;
        }
    }

    return last_sunday;
}

/* Calculate if we are in daylight saving time.
 * return  0 if false
 *         1 if true
 *        -1 in case of error
 */
static int is_dst(const struct tm *time) {
    /* DST from 01:00 UTC on last Sunday in March
     *     to   01:00 UTC on last Sunday in October
     */
    const int march = 2;
    const int october = 9;
    if (time->tm_mon < march) {
        return 0;
    }
    else if (time->tm_mon == march) {
        int last_sunday = find_last_sunday(time);
        if (last_sunday == -1) return -1;

        if (time->tm_mday < last_sunday) {
            return 0;
        }
        else if (time->tm_mday == last_sunday) {
            return (time->tm_hour < 1) ?  0 : 1;
        }
        else {
            return 1;
        }
    }
    else if (time->tm_mon > march && time->tm_mon < october) {
        return 1;
    }
    else if (time->tm_mon == october) {
        int last_sunday = find_last_sunday(time);
        if (last_sunday == -1) return -1;

        if (time->tm_mday < last_sunday) {
            return 1;
        }
        else if (time->tm_mday == last_sunday) {
            return (time->tm_hour < 1) ? 1 : 0;
        }
        else {
            return 0;
        }
    }
    else {
        return 0;
    }
}

int local_time(struct tm *time) {
    const int local_time_offset = 1; // hours

    int num_sv_used = 0;
    int valid = gps_utctime(time, &num_sv_used);

    if (valid) {
        time->tm_hour += local_time_offset;

        const int dst = is_dst(time);
        if (dst == -1) {
            usart_debug("mktime fail for dst %d-%d-%d %d:%d:%d "
                    "dst %d wday %d yday %d\r\n",
                    time->tm_year + 1900, time->tm_mon + 1, time->tm_mday,
                    time->tm_hour, time->tm_min, time->tm_sec,
                    time->tm_isdst, time->tm_wday, time->tm_yday);
        }
        else if (dst == 1) {
            time->tm_hour++;
            time->tm_isdst = 1;
        }

        // Let mktime fix the struct tm *time
        if (mktime(time) == (time_t)-1) {
            usart_debug("mktime fail for local_time %d-%d-%d %d:%d:%d "
                    "dst %d wday %d yday %d\r\n",
                    time->tm_year + 1900, time->tm_mon + 1, time->tm_mday,
                    time->tm_hour, time->tm_min, time->tm_sec,
                    time->tm_isdst, time->tm_wday, time->tm_yday);
            valid = 0;
        }

        if (valid) {
            last_derived_time = *time;
            last_derived_time_timestamp = timestamp_now();
            last_derived_time_valid = 1;
            last_derived_time_delta_applied = 0;
        }

    }

    return valid;
}

int local_derived_time(struct tm *time) {

    if (last_derived_time_valid == 0) {
        return 0;
    }

    // As there is a GPS timeout, local_time will think he has valid GPS data for GPS_MS_TIMEOUT. We need to remove it for better calculations.
    if (last_derived_time_delta_applied == 0) {
        last_derived_time_delta_applied = 1;
        last_derived_time_timestamp -= GPS_MS_TIMEOUT;
    }

    uint64_t new_timestamp = timestamp_now();

    while (new_timestamp - last_derived_time_timestamp > 1000) {
        last_derived_time.tm_sec += 1;
        last_derived_time_timestamp += 1000;
    }

    mktime(&last_derived_time);

    *time = last_derived_time;

    return 1;

}


void common_init(void)
{
    common_timer = xTimerCreate("Timer",
            pdMS_TO_TICKS(8),
            pdTRUE, // Auto-reload
            NULL,   // No unique id
            common_increase_timestamp
            );

    xTimerStart(common_timer, 0);

    lfsr = lfsr_start_state;
}

static void common_increase_timestamp(TimerHandle_t __attribute__ ((unused))t)
{

#ifdef SIMULATOR
    struct timespec ctime;

    clock_gettime(CLOCK_REALTIME, &ctime);
    common_timestamp = ctime.tv_sec * 1000 + ctime.tv_nsec / 1.0e6 - timestamp_delta;

    if (timestamp_delta == 0) {
        timestamp_delta = common_timestamp;
        common_timestamp = 0;
    }

#else
    common_timestamp += 8;
#endif
}

uint64_t timestamp_now(void)
{
    return common_timestamp; // ms
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

// For the debugger
static int faultsource = 0;
void trigger_fault(int source)
{
    usart_debug("Fatal: %d\r\n\r\n", source);

    __disable_irq();

    faultsource = source;

    while (1) {}
}

float round_float_to_half_steps(float value)
{
    return 0.5f * roundf(value * 2.0f);
}

