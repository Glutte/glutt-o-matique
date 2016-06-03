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

#include "../../../common/src/Core/main.c"

#include <pthread.h>


static void thread_gui(void *arg) {
    main_gui();
}


extern int gui_gps_send_frame;
extern int gui_gps_frames_valid;
extern char gui_gps_lat[64];
extern int gui_gps_lat_hem;
extern char gui_gps_lon[64];
extern int gui_gps_lon_hem;
extern int gui_gps_send_current_time;

static void thread_gui_gps(void *arg) {

    while(1) {
        vTaskDelay(1000 / portTICK_RATE_MS);

        if (gui_gps_send_frame) {

            char gps_frame_buffer[128];
            int gps_buffer_pointer = 0;

            strcpy(gps_frame_buffer + gps_buffer_pointer, "$GPRMC,");
            gps_buffer_pointer += 7;

            strcpy(gps_frame_buffer + gps_buffer_pointer, "225446,");
            gps_buffer_pointer += 7;

            if (gui_gps_frames_valid) {
                gps_frame_buffer[gps_buffer_pointer] = 'A';
            } else {
                gps_frame_buffer[gps_buffer_pointer] = 'V';
            }
            gps_buffer_pointer++;
            gps_frame_buffer[gps_buffer_pointer] = ',';
            gps_buffer_pointer++;


            gps_frame_buffer[gps_buffer_pointer] = '\0';
            printf("%s\n", gps_frame_buffer);

        }

    }
}

void init() {

    /* pthread_t pth; */
    /* pthread_create(&pth, NULL, thread_gui, "processing..."); */

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
