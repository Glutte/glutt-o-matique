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

#include "stm32f4xx_conf.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
#include "common.h"
#include "gps.h"
#include "i2c.h"
#include "ubx.h"


TickType_t gps_timeutc_last_updated = 0;
static struct gps_time_s gps_timeutc;

const TickType_t gps_data_validity_timeout = 10000ul / portTICK_PERIOD_MS;

TickType_t gps_fix_last_updated = 0;
static int gps_fix = 0;

static void gps_task(void *pvParameters);

// Callback functions for UBX parser
static void gps_nav_sol(ubx_nav_sol_t *sol)
{
    gps_fix_last_updated = xTaskGetTickCount();
    gps_fix = sol->GPSfix == GPSFIX_3D;
}

static void gps_tim_tm2(ubx_tim_tm2_t *posllh) {}

SemaphoreHandle_t timeutc_semaphore;
static void gps_nav_timeutc(ubx_nav_timeutc_t *timeutc)
{
    xSemaphoreTake(timeutc_semaphore, portMAX_DELAY);
    gps_timeutc_last_updated = xTaskGetTickCount();
    gps_timeutc.year  = timeutc->year;
    gps_timeutc.month = timeutc->month;
    gps_timeutc.day   = timeutc->day;
    gps_timeutc.hour  = timeutc->hour;
    gps_timeutc.min   = timeutc->min;
    gps_timeutc.sec   = timeutc->sec;
    gps_timeutc.valid = timeutc->valid;
    xSemaphoreGive(timeutc_semaphore);
}

// Get current time from GPS
void gps_utctime(struct gps_time_s *timeutc)
{
    xSemaphoreTake(timeutc_semaphore, portMAX_DELAY);
    if (gps_timeutc_last_updated + gps_data_validity_timeout < xTaskGetTickCount()) {
        timeutc->year  = gps_timeutc.year;
        timeutc->month = gps_timeutc.month;
        timeutc->day   = gps_timeutc.day;
        timeutc->hour  = gps_timeutc.hour;
        timeutc->min   = gps_timeutc.min;
        timeutc->sec   = gps_timeutc.sec;
        timeutc->valid = gps_timeutc.valid;
    }
    else {
        timeutc->valid = 0;
    }
    xSemaphoreGive(timeutc_semaphore);
}


const ubx_callbacks_t gps_ubx_cb = {
    gps_nav_sol,
    gps_nav_timeutc,
    gps_tim_tm2
};


#define RXBUF_LEN 128
static uint8_t rxbuf[RXBUF_LEN];
static uint8_t gps_init_messages[] = {
    UBX_ENABLE_NAV_SOL,
    UBX_ENABLE_NAV_TIMEUTC,
    UBX_ENABLE_TIM_TM2 };

static void gps_task(void *pvParameters)
{
    // Periodically reinit the GPS
    const TickType_t init_timeout = 10000ul / portTICK_PERIOD_MS;

    const uint8_t address = 0xFD;
    int must_init_gps = 1;

    TickType_t time_last_init = xTaskGetTickCount();

    while (1) {
        taskYIELD();

        if (time_last_init + init_timeout >= xTaskGetTickCount()) {
            must_init_gps = 1;
        }

        i2c_transaction_start();

        if (must_init_gps) {
            i2c_write(GPS_I2C_ADDR,
                    gps_init_messages,
                    sizeof(gps_init_messages));

            must_init_gps = 0;
        }

        int bytes_read = i2c_read_from(GPS_I2C_ADDR, address, rxbuf, 2);

        if (bytes_read != 2) {
            i2c_transaction_end();
            continue;
        }

        uint16_t bytes_available = (rxbuf[0] << 8) | rxbuf[1];

        if (bytes_available) {
            if (bytes_available > RXBUF_LEN) {
                bytes_available = RXBUF_LEN;
            }

            bytes_read = i2c_read(GPS_I2C_ADDR, rxbuf, bytes_available);
            i2c_transaction_end();

            for (int i = 0; i < bytes_read; i++) {
                ubx_parse(rxbuf[i]);
            }
        }
        else {
            i2c_transaction_end();
        }
    }
}

void gps_init()
{
    gps_timeutc.valid = 0;

    ubx_register(&gps_ubx_cb);

    timeutc_semaphore = xSemaphoreCreateBinary();

    if( timeutc_semaphore == NULL ) {
        trigger_fault(FAULT_SOURCE_GPS);
    }
    else {
        xSemaphoreGive(timeutc_semaphore);
    }

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
    if (gps_fix_last_updated + gps_data_validity_timeout < xTaskGetTickCount()) {
        return (gps_fix == GPSFIX_3D) ? 1 : 0;
    }
    else {
        return 0;
    }
}

