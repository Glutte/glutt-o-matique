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

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
#include "Core/common.h"
#include "GPS/gps.h"
#include "GPS/minmea.h"
#include "GPIO/usart.h"


TickType_t gps_timeutc_last_updated = 0;
static struct tm gps_timeutc;
static int gps_num_sv_used;
static int gps_timeutc_valid; // Validity flag for both gps_timeutc and gps_num_sv_used

const TickType_t gps_data_validity_timeout = GPS_MS_TIMEOUT / portTICK_PERIOD_MS;

static void gps_task(void *pvParameters);

SemaphoreHandle_t timeutc_semaphore;

// Get current time from GPS
int gps_utctime(struct tm *timeutc, int *num_sv_used)
{
    int valid = 0;

    xSemaphoreTake(timeutc_semaphore, portMAX_DELAY);
    if (xTaskGetTickCount() - gps_timeutc_last_updated < gps_data_validity_timeout) {
        timeutc->tm_year  = gps_timeutc.tm_year;
        timeutc->tm_mon   = gps_timeutc.tm_mon;
        timeutc->tm_mday  = gps_timeutc.tm_mday;
        timeutc->tm_hour  = gps_timeutc.tm_hour;
        timeutc->tm_min   = gps_timeutc.tm_min;
        timeutc->tm_sec   = gps_timeutc.tm_sec;
        *num_sv_used      = gps_num_sv_used;
        valid             = gps_timeutc_valid;
    }

    xSemaphoreGive(timeutc_semaphore);

    return valid;
}

#define RXBUF_LEN MAX_NMEA_SENTENCE_LEN
static char rxbuf[RXBUF_LEN];

static void gps_task(void __attribute__ ((unused))*pvParameters) {

    // The initialisation placed the GPS into reset
    usart_gps_remove_reset();

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
                            gps_timeutc.tm_year  = 2000 + frame.date.year;
                            // struct tm months are zero-indexed
                            gps_timeutc.tm_mon   = frame.date.month - 1;
                            gps_timeutc.tm_mday  = frame.date.day;
                            gps_timeutc.tm_hour  = frame.time.hours;
                            gps_timeutc.tm_min   = frame.time.minutes;
                            gps_timeutc.tm_sec   = frame.time.seconds;
                            gps_timeutc_valid    = frame.valid;
                            gps_timeutc_last_updated = xTaskGetTickCount();
                            xSemaphoreGive(timeutc_semaphore);
                        }
                    } break;
                case MINMEA_SENTENCE_TXT:
                    {
                        struct minmea_sentence_txt frame;
                        if (minmea_parse_txt(&frame, rxbuf)) {
                            rxbuf[MINMEA_TXT_START_IX + frame.text_len] = '\0';

                            switch (frame.msgtype) {
                                case MINMEA_GPTXT_ERROR:
                                    usart_debug_puts_header("GPS ERROR ", frame.text);
                                    break;
                                case MINMEA_GPTXT_WARNING:
                                    usart_debug_puts_header("GPS WARNING ", frame.text);
                                    break;
                                default:
                                    usart_debug_puts_header("GPS Message ", frame.text);
                                    break;
                            }
                        }
                    } break;
                case MINMEA_SENTENCE_GGA:
                    {
                        struct minmea_sentence_gga frame;
                        if (minmea_parse_gga(&frame, rxbuf)) {
                            xSemaphoreTake(timeutc_semaphore, portMAX_DELAY);
                            gps_num_sv_used = frame.satellites_tracked;
                            xSemaphoreGive(timeutc_semaphore);
                        }
                    } break;
                default:
                    break;
            }
        }
    }
}

void gps_init() {
    gps_timeutc_valid = 0;

    usart_gps_init();

    timeutc_semaphore = xSemaphoreCreateBinary();

    if (timeutc_semaphore == NULL) {
        trigger_fault(FAULT_SOURCE_GPS);
    } else {
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
int gps_locked() {
    if (xTaskGetTickCount() - gps_timeutc_last_updated < gps_data_validity_timeout) {
        return gps_timeutc_valid;
    } else {
        return 0;
    }
}
