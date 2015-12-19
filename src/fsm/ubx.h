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

/* u-blox UBX protocol parser, register callbacks for specific messages using
 * ubx_register() and and push in chars with ubx_parse()
 */

#ifndef __UBX_H_
#define __UBX_H_

#include <stdint.h>

// message sync bytes
#define UBX_SYNC1_CHAR  0xB5
#define UBX_SYNC2_CHAR  0x62

// UBX Class
typedef enum app_ubx_class_e // {{{
{
    UBX_CLASS_NAV = 0x01,       // Navigation Results: Position, Speed, Time, Acc, Heading, DOP, SVs used
    UBX_CLASS_RXM = 0x02,       // Receiver Manager Messages: Satellite Status, RTC Status
    UBX_CLASS_TRK = 0x03,       // Tracking Information: Measurements, Commands
    UBX_CLASS_INF = 0x04,       // Information Messages: Printf-Style Messages, with IDs such as Error, Warning, Notice
    UBX_CLASS_ACK = 0x05,       // Ack/Nack Messages: as replies to CFG Input Messages
    UBX_CLASS_CFG = 0x06,       // Configuration Input Messages: Set Dynamic Model, Set DOP Mask, Set Baud Rate, etc.
    UBX_CLASS_MON = 0x0A,       // Monitoring Messages: Communication Status, CPU Load, Stack Usage, Task Status
    UBX_CLASS_TIM = 0x0D,       // Timing Messages: Timepulse Output, Timemark Results
} app_ubx_class_t; //}}}

// UBX Message Id
typedef enum app_ubx_id_e // {{{
{
    // ACK
    UBX_ACK_NACK    = 0x00,         // Not acknowledge
    UBX_ACK_ACK     = 0x01,         // acknowledge

    // NAV
    UBX_NAV_STATUS  = 0x03,         // Navigation status
    UBX_NAV_SOL     = 0x06,         // Navigation solution
    UBX_NAV_PVT     = 0x07,         // Navigation PVT solution
    UBX_NAV_TIMEGPS = 0x20,         // GPS time
    UBX_NAV_TIMEUTC = 0x21,         // UTC time

    // TIM
    UBX_TIM_TM2     = 0x03,         // timemark data
} app_ubx_id_t; //}}}

// ubx parser state
typedef enum ubx_state_e
{
    UBXSTATE_SYNC1,
    UBXSTATE_SYNC2,
    UBXSTATE_CLASS,
    UBXSTATE_ID,
    UBXSTATE_LEN1,
    UBXSTATE_LEN2,
    UBXSTATE_DATA,
    UBXSTATE_CKA,
    UBXSTATE_CKB
} ubx_state_t;


typedef struct ubx_nav_sol_s
{
    uint32_t  itow;           // ms GPS Millisecond Time of Week
    int32_t   frac;           // ns remainder of rounded ms above
    int16_t   week;           // GPS week
    uint8_t   GPSfix;         // GPSfix Type, range 0..6
    uint8_t   Flags;          // Navigation Status Flags
    int32_t   ECEF_X;         // cm ECEF X coordinate
    int32_t   ECEF_Y;         // cm ECEF Y coordinate
    int32_t   ECEF_Z;         // cm ECEF Z coordinate
    int32_t   PAcc;           // cm 3D Position Accuracy Estimate
    int32_t   ECEFVX;         // cm/s ECEF X velocity
    int32_t   ECEFVY;         // cm/s ECEF Y velocity
    int32_t   ECEFVZ;         // cm/s ECEF Z velocity
    uint32_t  SAcc;           // cm/s Speed Accuracy Estimate
    uint16_t  PDOP;           // 0.01 Position DOP
    uint8_t   res1;           // reserved
    uint8_t   numSV;          // Number of SVs used in navigation solution
    uint32_t  res2;           // reserved
} __attribute__((packed)) ubx_nav_sol_t;


typedef struct ubx_nav_velned_s
{
    uint32_t  itow;     // ms  GPS Millisecond Time of Week
    int32_t   VEL_N;    // cm/s  NED north velocity
    int32_t   VEL_E;    // cm/s  NED east velocity
    int32_t   VEL_D;    // cm/s  NED down velocity
    int32_t   Speed;    // cm/s  Speed (3-D)
    int32_t   GSpeed;   // cm/s  Ground Speed (2-D)
    int32_t   Heading;  // 1e-05 deg  Heading 2-D
    uint32_t  SAcc;     // cm/s  Speed Accuracy Estimate
    uint32_t  CAcc;     // deg  Course / Heading Accuracy Estimate
} __attribute__((packed)) ubx_nav_velned_t;

typedef struct ubx_nav_posllh_s
{
    uint32_t  itow;    // ms GPS Millisecond Time of Week
    int32_t   LON;     // 1e-07 deg Longitude
    int32_t   LAT;     // 1e-07 deg Latitude
    int32_t   HEIGHT;  // mm Height above Ellipsoid
    int32_t   HMSL;    // mm Height above mean sea level
    uint32_t  Hacc;    // mm Horizontal Accuracy Estimate
    uint32_t  Vacc;    // mm Vertical Accuracy Estimate
} __attribute__((packed)) ubx_nav_posllh_t;

typedef struct ubx_tim_tm2_s
{
    uint8_t   ch;         // marker channel (0 or 1)
    uint8_t   flags;      // bitmask
    uint16_t  count;      // edge counter
    uint16_t  wnoR;       // week number of last rising edge
    uint16_t  wnoF;       // week number of last falling edge
    uint32_t  towMsR;     // tow in ms of last rising edge [ms]
    uint32_t  towSubMsR;  // millisecond fraction of tow of last rising edge [ns]
    uint32_t  towMsF;     // tow in ms of last rising edge [ms]
    uint32_t  towSubMsF;  // millisecond fraction of tow of last rising edge [ns]
    uint32_t  accEst;     // accuracy estimate [ns]
} __attribute__((packed)) ubx_tim_tm2_t;


typedef struct ubx_nav_timeutc_s
{
    uint32_t  itow;   // GPS Millisecond Time of Week
    uint32_t  tacc;   // time accuracy estimate [ns]
    int32_t   nano;   // Nanoseconds of second
    uint16_t  year;   // Year, range 1999..2099 (UTC)
    uint8_t   month;  // Month, range 1..12 (UTC)
    uint8_t   day;    // Day of Month, range 1..31 (UTC)
    uint8_t   hour;   // Hour of Day, range 0..23 (UTC)
    uint8_t   min;    // Minute of Hour, range 0..59 (UTC)
    uint8_t   sec;    // Seconds of Minute, range 0..59 (UTC)
    uint8_t   valid;  // validity flag
} __attribute__((packed)) ubx_nav_timeutc_t;


typedef struct ubx_callbacks_s
{
    void (*new_ubx_nav_sol)(ubx_nav_sol_t *sol);
    void (*new_ubx_nav_timeutc)(ubx_nav_timeutc_t *timeutc);
    void (*new_ubx_tim_tm2)(ubx_tim_tm2_t *posllh);
} ubx_callbacks_t;

/* Register callbacks when a complete message was received */
void ubx_register(const ubx_callbacks_t *cb);

/* Parse a new incoming byte, calls the callbacks if a complete message
 * was received. Returns the current parser state
 */
int ubx_parse(const uint8_t c);

#endif

