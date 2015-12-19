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

#include <stdint.h>

#ifndef __GPS_H_
#define __GPS_H_

/* Setup GPS receiver over I2C and parse time */

/* I2C connections:
 * SCL on PB6
 * SDA on PB9
 */

#define GPS_I2C_ADDR 0x42 // I2C address of GPS receiver

struct gps_time_s {
    uint16_t  year;   // Year, range 1999..2099 (UTC)
    uint8_t   month;  // Month, range 1..12 (UTC)
    uint8_t   day;    // Day of Month, range 1..31 (UTC)
    uint8_t   hour;   // Hour of Day, range 0..23 (UTC)
    uint8_t   min;    // Minute of Hour, range 0..59 (UTC)
    uint8_t   sec;    // Seconds of Minute, range 0..59 (UTC)
    uint8_t   valid;  // validity flag
};

// Setup communication and GPS receiver
void gps_init();

// Return 1 of the GPS is receiving time
int gps_locked();

// Get current time from GPS
void gps_utctime(struct gps_time_s *timeutc);

#endif // __GPS_H_

