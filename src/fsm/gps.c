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
#include "usart.h"
#include "minmea.h"


TickType_t gps_timeutc_last_updated = 0;
static struct gps_time_s gps_timeutc;

const TickType_t gps_data_validity_timeout = 10000ul / portTICK_PERIOD_MS;

static void gps_task(void *pvParameters);

SemaphoreHandle_t timeutc_semaphore;

// Get current time from GPS
void gps_utctime(struct gps_time_s *timeutc)
{
    xSemaphoreTake(timeutc_semaphore, portMAX_DELAY);
    if (xTaskGetTickCount() - gps_timeutc_last_updated < gps_data_validity_timeout) {
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

#define RXBUF_LEN MAX_NMEA_SENTENCE_LEN
static char rxbuf[RXBUF_LEN];

static void gps_task(void *pvParameters)
{
    // Periodically reinit the GPS
    while (1) {
        taskYIELD();

        int success = usart_get_nmea_sentence(rxbuf);

        if (success) {
            const int strict = 1;
            switch (minmea_sentence_id(rxbuf, strict)) {
                case MINMEA_SENTENCE_RMC:
                    {
                        struct minmea_sentence_rmc frame;
                        if (minmea_parse_rmc(&frame, rxbuf)) {
                            xSemaphoreTake(timeutc_semaphore, portMAX_DELAY);
                            gps_timeutc_last_updated = xTaskGetTickCount();
                            gps_timeutc.year  = 2000 + frame.date.year;
                            gps_timeutc.month = frame.date.month;
                            gps_timeutc.day   = frame.date.day;
                            gps_timeutc.hour  = frame.time.hours;
                            gps_timeutc.min   = frame.time.minutes;
                            gps_timeutc.sec   = frame.time.seconds;
                            gps_timeutc.valid = frame.valid;
                            xSemaphoreGive(timeutc_semaphore);
                        }
                    } break;
                default:
                    break;
            }
        }
    }
}

void gps_init()
{
    gps_timeutc.valid = 0;

    usart_init();

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
    if (xTaskGetTickCount() - gps_timeutc_last_updated < gps_data_validity_timeout) {
        return gps_timeutc.valid;
    }
    else {
        return 0;
    }
}

