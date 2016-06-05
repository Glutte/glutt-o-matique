/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Matthias P. Braendli, Maximilien Cuony
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

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "src/GPS/gps_sim.h"
#include "src/Gui/gui.h"


void init(void);


static void thread_gui(void __attribute__ ((unused))*arg) {
    main_gui();
}


extern int gui_gps_send_frame;
extern int gui_gps_frames_valid;
extern char gui_gps_lat[64];
extern int gui_gps_lat_len;
extern int gui_gps_lat_hem;
extern char gui_gps_lon[64];
extern int gui_gps_lon_len;
extern int gui_gps_lon_hem;
extern int gui_gps_send_current_time;

int gui_gps_custom_hour_on;
int gui_gps_custom_min_on;
int gui_gps_custom_sec_on;
int gui_gps_custom_day_on;
int gui_gps_custom_month_on;
int gui_gps_custom_year_on;
char gui_gps_custom_hour[4];
int gui_gps_custom_hour_len;
char gui_gps_custom_min[4];
int gui_gps_custom_min_len;
char gui_gps_custom_sec[4];
int gui_gps_custom_sec_len;
char gui_gps_custom_day[4];
int gui_gps_custom_day_len;
char gui_gps_custom_month[4];
int gui_gps_custom_month_len;
char gui_gps_custom_year[4];
int gui_gps_custom_year_len;

static void thread_gui_gps(void __attribute__ ((unused))*arg) {

    while(1) {
        vTaskDelay(1000 / portTICK_RATE_MS);

        if (gui_gps_send_frame) {

            time_t now = time(NULL);
            struct tm *t = gmtime(&now);

            char gps_frame_buffer[128];
            int gps_buffer_pointer = 0;

            strcpy(gps_frame_buffer + gps_buffer_pointer, "$GNRMC,");
            gps_buffer_pointer += 7;

            if (gui_gps_send_current_time) {

                sprintf(gps_frame_buffer + gps_buffer_pointer, "%02d%02d%02d", t->tm_hour, t->tm_min, t->tm_sec);

            } else {

                int hour = atoi(gui_gps_custom_hour);
                int min = atoi(gui_gps_custom_min);
                int sec = atoi(gui_gps_custom_sec);

                if (!gui_gps_custom_hour_on) {
                    hour = t->tm_hour;
                }
                if (!gui_gps_custom_min_on) {
                    min = t->tm_min;
                }
                if (!gui_gps_custom_sec_on) {
                    sec = t->tm_sec;
                }

                sprintf(gps_frame_buffer + gps_buffer_pointer, "%02d%02d%02d", hour, min, sec);
            }
            gps_buffer_pointer += 6;

            gps_frame_buffer[gps_buffer_pointer] = ',';
            gps_buffer_pointer++;

            if (gui_gps_frames_valid) {
                gps_frame_buffer[gps_buffer_pointer] = 'A';
            } else {
                gps_frame_buffer[gps_buffer_pointer] = 'V';
            }
            gps_buffer_pointer++;

            gps_frame_buffer[gps_buffer_pointer] = ',';
            gps_buffer_pointer++;

            strcpy(gps_frame_buffer + gps_buffer_pointer, gui_gps_lat);
            gps_buffer_pointer += gui_gps_lat_len;

            gps_frame_buffer[gps_buffer_pointer] = ',';
            gps_buffer_pointer++;

            if (gui_gps_lat_hem == 0) {
                gps_frame_buffer[gps_buffer_pointer] = 'N';
            } else {
                gps_frame_buffer[gps_buffer_pointer] = 'S';
            }
            gps_buffer_pointer++;

            gps_frame_buffer[gps_buffer_pointer] = ',';
            gps_buffer_pointer++;

            strcpy(gps_frame_buffer + gps_buffer_pointer, gui_gps_lon);
            gps_buffer_pointer += gui_gps_lon_len;

            gps_frame_buffer[gps_buffer_pointer] = ',';
            gps_buffer_pointer++;

            if (gui_gps_lon_hem == 0) {
                gps_frame_buffer[gps_buffer_pointer] = 'E';
            } else {
                gps_frame_buffer[gps_buffer_pointer] = 'W';
            }
            gps_buffer_pointer++;

            strcpy(gps_frame_buffer + gps_buffer_pointer, ",,,");
            gps_buffer_pointer += 3;

            if (gui_gps_send_current_time) {

                sprintf(gps_frame_buffer + gps_buffer_pointer, "%02d%02d%02d", t->tm_mday, t->tm_mon, t->tm_year - 100);

            } else {
                strcpy(gps_frame_buffer + gps_buffer_pointer, "000000");

                int day = atoi(gui_gps_custom_day);
                int mon = atoi(gui_gps_custom_month);
                int year = atoi(gui_gps_custom_year);

                if (!gui_gps_custom_day_on) {
                    day = t->tm_mday;
                }
                if (!gui_gps_custom_month_on) {
                    mon = t->tm_mon;
                }
                if (!gui_gps_custom_year_on) {
                    year = t->tm_year - 100;
                }

                sprintf(gps_frame_buffer + gps_buffer_pointer, "%02d%02d%02d", day, mon, year);
            }
            gps_buffer_pointer += 6;

            strcpy(gps_frame_buffer + gps_buffer_pointer, ",,,A*");
            gps_buffer_pointer += 5;


            uint8_t checksum = 0;
            int i = 1;

            while (gps_frame_buffer[i] && gps_frame_buffer[i] != '*') {
                checksum ^= gps_frame_buffer[i];
                i++;
            }

            sprintf(gps_frame_buffer + gps_buffer_pointer, "%02x\n", checksum);
            gps_buffer_pointer += 3;

            gps_frame_buffer[gps_buffer_pointer] = '\0';

            gps_usart_send(gps_frame_buffer);
            /* printf(gps_frame_buffer); */

        }

    }
}

void init() {

    TaskHandle_t task_handle;
    xTaskCreate(
            thread_gui,
            "Thread GUI",
            configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            &task_handle);

    xTaskCreate(
            thread_gui_gps,
            "Thread GUI GPS",
            configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            &task_handle);

}
