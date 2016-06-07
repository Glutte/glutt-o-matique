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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Includes */
#include "Audio/audio.h"
#include "Audio/cw.h"
#include "GPIO/pio.h"
#include "GPIO/i2c.h"
#include "GPS/gps.h"
#include "Core/fsm.h"
#include "Core/common.h"
#include "GPIO/usart.h"
#include "Core/delay.h"
#include "GPIO/temperature.h"
#include "GPIO/leds.h"
#include "vc.h"


// Private variables
static int tm_trigger_button = 0;
static int tm_trigger = 0;

// Private function prototypes
void init();

// Tasks
static void detect_button_press(void *pvParameters);
static void exercise_fsm(void *pvParameters);
static void gps_monit_task(void *pvParameters);
static void launcher_task(void *pvParameters);

// Audio callback function
static void audio_callback(void* context, int select_buffer);

void vApplicationStackOverflowHook(TaskHandle_t, signed char *);

void vApplicationStackOverflowHook(TaskHandle_t __attribute__ ((unused)) xTask, signed char *pcTaskName) {
    usart_debug("TASK OVERFLOW %s\r\n", pcTaskName);
    while (1) {};
}

int main(void) {
    init();
    delay_init();
    usart_init();
    usart_debug_puts("\r\n******* glutt-o-matique version " GIT_VERSION " *******\r\n");

#ifndef SIMULATOR

    if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
    {
        usart_debug_puts("WARNING: A IWDG Reset occured!\r\n");
    }
    RCC_ClearFlag();

#endif

    TaskHandle_t task_handle;
    xTaskCreate(
            launcher_task,
            "Launcher",
            configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            &task_handle);

    if (!task_handle) {
        trigger_fault(FAULT_SOURCE_MAIN);
    }

    vTaskStartScheduler();

    /* HALT */
    while(1);
}


static void test_task(void __attribute__ ((unused))*pvParameters) {

    int i = 0;

    while(1) {
        vTaskDelay(1000 / portTICK_RATE_MS);

        if (i == 0) {
            i = 1;
            leds_turn_on(LED_GREEN);

        } else {
            i = 0;
            leds_turn_off(LED_GREEN);
        }

    }

}

// Launcher task is here to make sure the scheduler is
// already running when calling the init functions.
static void launcher_task(void __attribute__ ((unused))*pvParameters)
{
    usart_debug_puts("CW init\r\n");
    cw_psk31_init(16000);

    usart_debug_puts("PIO init\r\n");
    pio_init();

    usart_debug_puts("I2C init\r\n");
    i2c_init();

    usart_debug_puts("common init\r\n");
    common_init();

    usart_debug_puts("GPS init\r\n");
    gps_init();

    usart_debug_puts("DS18B20 init\r\n");
    temperature_init();

    usart_debug_puts("TaskButton init\r\n");

    TaskHandle_t task_handle;
    xTaskCreate(
            detect_button_press,
            "TaskButton",
            4*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            &task_handle);

    if (!task_handle) {
        trigger_fault(FAULT_SOURCE_MAIN);
    }

    usart_debug_puts("TaskFSM init\r\n");

    xTaskCreate(
            exercise_fsm,
            "TaskFSM",
            4*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            &task_handle);

    if (!task_handle) {
        trigger_fault(FAULT_SOURCE_MAIN);
    }

    usart_debug_puts("TaskGPS init\r\n");

    xTaskCreate(
            gps_monit_task,
            "TaskGPSMonit",
            4*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            &task_handle);

    if (!task_handle) {
        trigger_fault(FAULT_SOURCE_MAIN);
    }

    usart_debug_puts("Audio init\r\n");

    audio_initialize(Audio16000HzSettings);

    usart_debug_puts("Audio set volume\r\n");
    audio_set_volume(210);

    usart_debug_puts("Audio set callback\r\n");
    audio_play_with_callback(audio_callback, NULL);

    // By default, let's the audio off to save power
    audio_off();

    usart_debug_puts("Init done.\r\n");

    xTaskCreate(
            test_task,
            "Test task",
            4*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            &task_handle);

    /* We are done now, suspend this task
     * With FreeDOS' heap_1.c, we cannot delete it.
     * See freertos.org -> More Advanced... -> Memory Management
     * for more info.
     */

    while (1) {
        vTaskSuspend(NULL);
    }
}


static void detect_button_press(void __attribute__ ((unused))*pvParameters)
{
    int pin_high_count = 0;
    int last_pin_high_count = 0;
    const int pin_high_thresh = 10;
    while (1) {
        if (pio_read_button()) {
            if (pin_high_count < pin_high_thresh) {
                pin_high_count++;
            }
        }
        else {
            if (pin_high_count > 0) {
                pin_high_count--;
            }
        }

        vTaskDelay(10 / portTICK_RATE_MS); /* Debounce Delay */

        if (pin_high_count == pin_high_thresh &&
                last_pin_high_count != pin_high_count) {
            tm_trigger_button = 1;
            usart_debug_puts("Bouton bleu\r\n");

            if (temperature_valid()) {

                float temp = temperature_get();

                usart_debug("Temperature %f\r\n", temp);

            } else {
                usart_debug_puts("No temp\r\n");
            }
        }
        else if (pin_high_count == 0 &&
                last_pin_high_count != pin_high_count) {
            tm_trigger_button = 0;
        }

        last_pin_high_count = pin_high_count;
    }
}

static void audio_callback(void __attribute__ ((unused))*context, int select_buffer) {
    static int16_t audio_buffer0[AUDIO_BUF_LEN];
    static int16_t audio_buffer1[AUDIO_BUF_LEN];
    int16_t *samples;

    if (select_buffer == 0) {
        samples = audio_buffer0;
        leds_turn_off(LED_RED);
        select_buffer = 1;
    } else {
        samples = audio_buffer1;
        leds_turn_on(LED_RED);
        select_buffer = 0;
    }

    size_t samples_len = cw_psk31_fill_buffer(samples, AUDIO_BUF_LEN);

    if (samples_len == 0) {
        for (int i = 0; i < AUDIO_BUF_LEN; i++) {
            samples[i] = 0;
        }

        samples_len = AUDIO_BUF_LEN;
    }

    audio_provide_buffer_without_blocking(samples, samples_len);

}

static struct tm gps_time;
static void gps_monit_task(void __attribute__ ((unused))*pvParameters) {

    leds_turn_on(LED_BLUE);

    int t_gps_print_latch = 0;
    int t_gps_hours_handeled = 0;
    uint64_t last_hour_timestamp = 0;


    while (1) {
        struct tm time;
        int time_valid = local_time(&time);

        if (time_valid) {
            if (time.tm_sec % 4 >= 2) {
                leds_turn_on(LED_BLUE);
            }
            else {
                leds_turn_off(LED_BLUE);
            }

            // Even hours: tm_trigger=1, odd hours: tm_trigger=0
            tm_trigger = (time.tm_hour + 1) % 2;
        }

        gps_utctime(&gps_time);

        if (gps_time.tm_sec % 30 == 0 && t_gps_print_latch == 0) {
            usart_debug("T_GPS %04d-%02d-%02d %02d:%02d:%02d\r\n",
                    gps_time.tm_year, gps_time.tm_mon, gps_time.tm_mday,
                    gps_time.tm_hour, gps_time.tm_min, gps_time.tm_sec);

            usart_debug("TIME  %04d-%02d-%02d %02d:%02d:%02d\r\n",
#ifdef SIMULATOR
                    time.tm_year + 1900,
#else
                    time.tm_year,
#endif
                    time.tm_mon, time.tm_mday,
                    time.tm_hour, time.tm_min, time.tm_sec);


            t_gps_print_latch = 1;
        }
        if (gps_time.tm_sec % 30 > 0) {
            t_gps_print_latch = 0;
        }

        if (time_valid && gps_time.tm_sec == 0 && gps_time.tm_min == 0 && t_gps_hours_handeled == 0) {

            uint64_t current_timestamp = timestamp_now();

            if (last_hour_timestamp == 0) {
                usart_debug("DERIV INIT TS=%i\r\n", current_timestamp);
            } else {

                usart_debug("DERIV TS=%lld Excepted=%lld Delta=%lld\r\n",
                    current_timestamp,
                    last_hour_timestamp + 3600000,
                    last_hour_timestamp + 3600000 - current_timestamp
                );
            }

            last_hour_timestamp = timestamp_now();

            t_gps_hours_handeled = 1;
        }

        if (gps_time.tm_sec != 0) {
            t_gps_hours_handeled = 0;
        }

        vTaskDelay(100 / portTICK_RATE_MS);

        // Reload watchdog
#ifndef SIMULATOR
        IWDG_ReloadCounter();
#endif
    }
}

static struct fsm_input_signals_t fsm_input;
static void exercise_fsm(void __attribute__ ((unused))*pvParameters)
{
    int cw_last_trigger = 0;
    int last_tm_trigger = 0;
    int last_tm_trigger_button = 0;

    int last_sq = 0;
    int last_1750 = 0;
    int last_qrp = 0;
    int last_cw_done = 0;

    fsm_input.humidity = 0;
    fsm_input.temp = 15;
    fsm_input.swr_high = 0;
    fsm_input.sstv_mode = 0;
    fsm_input.wind_generator_ok = 1;

    audio_on();

    while (1) {
        vTaskDelay(10 / portTICK_RATE_MS);

        pio_set_fsm_signals(&fsm_input);

        if (last_sq != fsm_input.sq) {
            last_sq = fsm_input.sq;
            usart_debug("In SQ %d\r\n", last_sq);
        }
        if (last_1750 != fsm_input.tone_1750) {
            last_1750 = fsm_input.tone_1750;
            usart_debug("In 1750 %d\r\n", last_1750);
        }
        if (last_qrp != fsm_input.qrp) {
            last_qrp = fsm_input.qrp;
            usart_debug("In QRP %d\r\n", last_qrp);
        }


        if (tm_trigger_button == 1 && last_tm_trigger_button == 0) {
            fsm_input.start_tm = 1;
        }
        last_tm_trigger_button = tm_trigger_button;

        if (tm_trigger == 1 && last_tm_trigger == 0) {
            fsm_input.start_tm = 1;
        }
        last_tm_trigger = tm_trigger;

        int cw_done = !cw_psk31_busy();
        if (last_cw_done != cw_done) {
            usart_debug("In CW done %d\r\n", cw_done);
            last_cw_done = cw_done;

            fsm_input.cw_psk31_done = cw_done;
        }
        else {
            fsm_input.cw_psk31_done = 0;
        }

        if (fsm_input.cw_psk31_done) {
            leds_turn_off(LED_ORANGE);
        }
        else {
            leds_turn_on(LED_ORANGE);
        }

        fsm_update_inputs(&fsm_input);
        fsm_update();

        struct fsm_output_signals_t fsm_out;
        fsm_get_outputs(&fsm_out);

        pio_set_tx(fsm_out.tx_on);
        pio_set_mod_off(!fsm_out.modulation);
        pio_set_qrp(fsm_out.qrp); // TODO move out of FSM

        // Add message to CW generator only on rising edge of trigger
        if (fsm_out.cw_psk31_trigger && !cw_last_trigger) {
            cw_psk31_push_message(fsm_out.msg, fsm_out.cw_dit_duration, fsm_out.msg_frequency);

            usart_debug_puts("Out CW trigger\r\n");
        }
        cw_last_trigger = fsm_out.cw_psk31_trigger;

        if (fsm_out.ack_start_tm) {
            fsm_input.start_tm = 0;
        }
    }
}
