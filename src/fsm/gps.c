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

#include "gps.h"
#include "i2c.h"
#include "string.h"
#include "stm32f4xx_conf.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"

static ubx_nav_timeutc_t gps_timeutc;

static void gps_task(void *pvParameters);

// Callback functions for UBX parser
static void gps_nav_sol(ubx_nav_sol_t *sol) {}
static void gps_tim_tm2(ubx_tim_tm2_t *posllh) {}
static void gps_nav_timeutc(ubx_nav_timeutc_t *timeutc)
{
    memcpy(&gps_timeutc, timeutc, sizeof(ubx_nav_timeutc_t));
}

const ubx_callbacks_t gps_ubx_cb = {
    gps_nav_sol,
    gps_nav_timeutc,
    gps_tim_tm2
};


#define RXBUF_LEN 32
static uint8_t rxbuf[RXBUF_LEN];

static void gps_task(void *pvParameters)
{
    while (1) {
        const uint8_t address = 0xFF;
        int bytes_read = i2c_read(GPS_I2C_ADDR, address, rxbuf, RXBUF_LEN);

        for (int i = 0; i < bytes_read; i++) {
            ubx_parse(rxbuf[i]);
        }
    }
}

void gps_init()
{
    i2c_init();
    ubx_register(&gps_ubx_cb);

    xTaskCreate(
            gps_task,
            "TaskGPS",
            4*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            NULL);
}

// Return 1 of the GPS is receiving time
int gps_locked()
{
    return 0;
}

// Get current time from GPS
ubx_nav_timeutc_t* gps_utctime()
{

    return &gps_timeutc;
}

