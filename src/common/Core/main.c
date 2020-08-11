/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Matthias P. Braendli, Maximilien Cuony
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
#include <string.h>
#include <math.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Includes */
#include "Audio/audio.h"
#include "Audio/audio_in.h"
#include "Audio/tone.h"
#include "Audio/cw.h"
#include "GPIO/pio.h"
#include "GPIO/i2c.h"
#include "GPS/gps.h"
#include "Core/fsm.h"
#include "Core/stats.h"
#include "Core/common.h"
#include "GPIO/usart.h"
#include "GPIO/batterycharge.h"
#include "Core/delay.h"
#include "GPIO/temperature.h"
#include "GPIO/leds.h"
#include "GPIO/analog.h"
#include "vc.h"

#ifdef SIMULATOR
extern int gui_in_tone_1750;
#endif

static void print_task_stats(void);

static int tm_trigger_button = 0;

static struct fsm_input_signals_t fsm_input;
static int hour_is_even = 0;

/* Threshold for SWR measurement */
const int swr_refl_threshold = 10; // mV

/* Saturating counter for SWR measurement
 *
 * Hysteresis:
 * If the counter reaches the max value, the
 * SWR Error is triggered.
 *
 * The error can only be cleared through
 * a full reset.
 */
#define SWR_ERROR_COUNTER_MAX 10
static int swr_error_counter = 0;
static int swr_error_flag = 0;

// Platform specific init function
void init(void);

// Tasks
static void detect_button_press(void *pvParameters);
static void exercise_fsm(void *pvParameters);
static void nf_analyse(void *pvParameters);
static void read_in_coulomb_counter(void *pvParameters);
static void gps_monit_task(void *pvParameters);
static void launcher_task(void *pvParameters);

// Audio callback function
static void audio_callback(void* context, int select_buffer);
// Debugging
static uint64_t timestamp_last_audio_callback = 0;

void vApplicationStackOverflowHook(TaskHandle_t, signed char *);

void vApplicationStackOverflowHook(TaskHandle_t __attribute__ ((unused)) xTask, signed char *pcTaskName) {
    usart_debug("TASK OVERFLOW %s\r\n", pcTaskName);
    trigger_fault(FAULT_SOURCE_TASK_OVERFLOW);
}

int main(void) {
    init();
    delay_init();
    usart_init();
    usart_debug("\r\n******* glutt-o-matique version %s *******\r\n", vc_get_version());

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
            2*configMINIMAL_STACK_SIZE,
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


// Launcher task is here to make sure the scheduler is
// already running when calling the init functions.
static void launcher_task(void __attribute__ ((unused))*pvParameters)
{
    usart_debug_puts("CW init\r\n");
    cw_psk_init(16000);

    usart_debug_puts("PIO init\r\n");
    pio_init();

    usart_debug_puts("Analog init\r\n");
    analog_init();

    usart_debug_puts("Batterycharge init\r\n");
    batterycharge_init();

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

    usart_debug_puts("Tone init\r\n");
    tone_init();

    usart_debug_puts("Audio in init\r\n");
    audio_in_initialize();

    usart_debug_puts("TaskNF init\r\n");

    xTaskCreate(
            nf_analyse,
            "TaskNF",
            3*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 3UL,
            &task_handle);

    if (!task_handle) {
        trigger_fault(FAULT_SOURCE_MAIN);
    }

    usart_debug_puts("TaskCC init\r\n");

    xTaskCreate(
            read_in_coulomb_counter,
            "TaskCC",
            2*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 3UL,
            &task_handle);

    if (!task_handle) {
        trigger_fault(FAULT_SOURCE_MAIN);
    }

    usart_debug_puts("Init done.\r\n");

    int last_qrp_from_supply = 0;
    int send_audio_callback_warning = 0;

    int i = 0;

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (i == 0) {
            i = 1;
            leds_turn_on(LED_GREEN);

            print_task_stats();
        }
        else {
            i = 0;
            leds_turn_off(LED_GREEN);
        }

        struct fsm_output_signals_t fsm_out;
        fsm_get_outputs(&fsm_out);

        tone_detector_enable(fsm_out.require_tone_detector);

        if (fsm_out.tx_on) {
            int swr_fwd_mv, swr_refl_mv;
            if (analog_measure_swr(&swr_fwd_mv, &swr_refl_mv)) {
                if (swr_refl_mv > swr_refl_threshold) {
                    usart_debug("SWR meas %d mV\r\n", swr_refl_mv);
                    swr_error_counter++;
                }
                else {
                    swr_error_counter--;
                }

                if (swr_error_counter > SWR_ERROR_COUNTER_MAX) {
                    swr_error_counter = SWR_ERROR_COUNTER_MAX;
                    if (!swr_error_flag) {
                        usart_debug("Set SWR error\r\n");
                    }
                    swr_error_flag = 1;
                    pio_set_qrp(1);
                }
            }
        }
        else {
            if (swr_error_flag) {
                pio_set_qrp(1);
            }
            else {
                const uint32_t charge_qrp = batterycharge_too_low();

                if (charge_qrp != -1) {
                    if (charge_qrp != last_qrp_from_supply) {
                        usart_debug("QRP CC = %d\r\n", charge_qrp);
                        last_qrp_from_supply = charge_qrp;

                        pio_set_qrp(charge_qrp);
                    }
                }
                else {
                    /* Read the voltage when battery capacity is not available */
                    const int qrp_from_supply = analog_supply_too_low();
                    if (qrp_from_supply != last_qrp_from_supply) {
                        usart_debug("QRP U = %d\r\n", qrp_from_supply);
                        last_qrp_from_supply = qrp_from_supply;

                        pio_set_qrp(qrp_from_supply);
                    }
                }
            }
        }

        if (timestamp_now() - timestamp_last_audio_callback > 1000) {
            if (send_audio_callback_warning == 0) {
                send_audio_callback_warning = 1;
                usart_debug("[HOHO] timestamp_last_audio_callback > 1000\r\n");
            }
        }
        else {
            if (send_audio_callback_warning == 1) {
                send_audio_callback_warning = 0;
                usart_debug("[HOHO] Fix ? Now timestamp_last_audio_callback < 1000\r\n");
            }
        }
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

        vTaskDelay(pdMS_TO_TICKS(10)); /* Debounce Delay */

        if (pin_high_count == pin_high_thresh &&
                last_pin_high_count != pin_high_count) {
            tm_trigger_button = 1;
            usart_debug_puts("Bouton bleu\r\n");
        }
        else if (pin_high_count == 0 &&
                last_pin_high_count != pin_high_count) {
            tm_trigger_button = 0;
        }

        last_pin_high_count = pin_high_count;
    }
}

int only_zero_in_audio_buffer = 1;
int count_zero_audio_buffer = 0;

static void audio_callback(void __attribute__ ((unused))*context, int select_buffer) {
    static int16_t audio_buffer0[AUDIO_BUF_LEN];
    static int16_t audio_buffer1[AUDIO_BUF_LEN];
    int16_t *samples;

    if (select_buffer == 0) {
        samples = audio_buffer0;
        leds_turn_off(LED_RED);
    } else {
        samples = audio_buffer1;
        leds_turn_on(LED_RED);
    }

    size_t samples_len = cw_psk_fill_buffer(samples, AUDIO_BUF_LEN);

    if (samples_len == 0) {
        for (int i = 0; i < AUDIO_BUF_LEN; i++) {
            samples[i] = 0;
        }

        samples_len = AUDIO_BUF_LEN;

        if (count_zero_audio_buffer < 2) {
            count_zero_audio_buffer++;
        } else {
            only_zero_in_audio_buffer = 1;
        }
    } else {
        only_zero_in_audio_buffer = 0;
        count_zero_audio_buffer = 0;
    }

    if (!audio_provide_buffer_without_blocking(samples, samples_len)) {
        usart_debug("[HOHO] audio_provide_buffer_without_blocking returned False.\r\n");
    }

    timestamp_last_audio_callback = timestamp_now();
}

static struct tm gps_time;
static void gps_monit_task(void __attribute__ ((unused))*pvParameters) {

    /* There are two types of non GPS clocks: the DERIVED one which works if
     * GPS time was known at some point, and the free-running that only depends
     * on timestamp_now(). The free-running one is used to ensure 2h beacons are
     * transmitted even if GPS never gave us time. The DERIVED kicks in when GPS
     * fails after having output time information and tries to keep accurate absolute
     * time.
     */

    pio_set_gps_epps(1);

    int t_gps_print_latch = 0;
    int t_gps_hours_handeled = 0;
    uint64_t last_hour_timestamp = 0;

    uint64_t last_volt_and_temp_timestamp, last_hour_is_even_change_timestamp;
    last_volt_and_temp_timestamp = last_hour_is_even_change_timestamp = timestamp_now();

    int last_even = -1;

    while (1) {
        const uint64_t now = timestamp_now();

        struct tm time = {0};
        int time_valid = local_time(&time);
        int derived_mode = 0;

        if (time_valid) {
            if (time.tm_sec % 2) {
                pio_set_gps_epps(1);
            }
            else {
                pio_set_gps_epps(0);
            }

            derived_mode = 0;
        }
        else {
            time_valid = local_derived_time(&time);

            if (time_valid) {
                if (time.tm_sec % 4 >= 2) {
                    pio_set_gps_epps(1);
                }
                else {
                    pio_set_gps_epps(0);
                }

                derived_mode = 1;
            }
        }

        if (time_valid) {
            hour_is_even = (time.tm_hour + 1) % 2;

            if (last_even != hour_is_even) {
                last_even = hour_is_even;

                usart_debug("Even changed: %i %i %s\r\n", hour_is_even, time.tm_hour, derived_mode ? "DERIVED" : "GPS");
            }
        }
        else if (last_hour_is_even_change_timestamp + (2 * 3600 * 1000) < now) {
            hour_is_even = (hour_is_even + 1) % 2;
            last_even = hour_is_even;

            usart_debug("Even changed: %i %i FREE-RUNNING\r\n", hour_is_even, time.tm_hour);
            last_hour_is_even_change_timestamp = now;
        }

        if (last_volt_and_temp_timestamp + 20000 < now) {
            const float u_bat = analog_measure_12v();
            const uint32_t capacity_bat = batterycharge_retrieve_last_capacity();

            usart_debug("ALIM %d mV\r\n", (int)roundf(1000.0f * u_bat));

            stats_voltage(u_bat);
            if (time_valid && time.tm_min == 0) {
                stats_battery_at_full_hour(time.tm_hour, u_bat, capacity_bat);
            }

            if (temperature_valid()) {
                const float temp = temperature_get();
                stats_temp(temp);
                usart_debug("TEMP %d.%02d\r\n", (int)temp, (int)(temp * 100.0f - (int)(temp) * 100.0f));
            }
            else {
                usart_debug("TEMP invalid\r\n");
            }

            last_volt_and_temp_timestamp = now;
        }

        int num_sv_used = 0;
        gps_utctime(&gps_time, &num_sv_used);

        if (time.tm_sec % 30 == 0 && t_gps_print_latch == 0) {
            usart_debug("T_GPS %04d-%02d-%02d %02d:%02d:%02d %d SV tracked\r\n",
                gps_time.tm_year + 1900, gps_time.tm_mon + 1, gps_time.tm_mday,
                gps_time.tm_hour, gps_time.tm_min, gps_time.tm_sec,
                num_sv_used);

            stats_num_gnss_sv(num_sv_used);

            char *mode = "";

            if (derived_mode) {
                mode = "Derived";
            }
            else {
                mode = "GPS";
            }

            usart_debug("TIME  %04d-%02d-%02d %02d:%02d:%02d [%s]\r\n",
                time.tm_year + 1900,
                time.tm_mon + 1, time.tm_mday,
                time.tm_hour, time.tm_min, time.tm_sec,
                mode);

            t_gps_print_latch = 1;
        }

        if (time.tm_sec % 30 > 0) {
            t_gps_print_latch = 0;
        }

        if (time_valid && derived_mode == 0 && gps_time.tm_sec == 0 && gps_time.tm_min == 0 && t_gps_hours_handeled == 0) {
            if (last_hour_timestamp == 0) {
                usart_debug("DERIV INIT TS=%lld\r\n", now);
            }
            else {
                usart_debug("DERIV TS=%lld Excepted=%lld Delta=%lld\r\n",
                    now,
                    last_hour_timestamp + 3600000,
                    last_hour_timestamp + 3600000 - now
                );
            }

            last_hour_timestamp = now;

            t_gps_hours_handeled = 1;
        }

        if (gps_time.tm_sec != 0) {
            t_gps_hours_handeled = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(100));

        // Reload watchdog
#ifndef SIMULATOR
        IWDG_ReloadCounter();
#endif
    }
}

static void exercise_fsm(void __attribute__ ((unused))*pvParameters)
{
    fsm_init();

    int cw_last_trigger = 0;
    int last_tm_trigger_button = 0;

    int last_tx_on = 0;
    int last_sq = 0;
    int last_qrp = 0;
    int last_cw_done = 0;
    int last_discrim_d = 0;
    int last_discrim_u = 0;
    int last_wind_generator_ok = 0;

    uint64_t last_qrp_stats_updated = timestamp_now();

    fsm_input.humidity = 0;
    fsm_input.temp = 15;
    fsm_input.swr_high = 0;
    fsm_input.fax_mode = 0;
    fsm_input.wind_generator_ok = 1;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));

        pio_set_fsm_signals(&fsm_input);

        const uint64_t now = timestamp_now();

        // QRP/QRO doesn't change too often, updating every 10s is good enough
        if (last_qrp_stats_updated + 10000 < now) {
            stats_qrp(fsm_input.qrp);
            last_qrp_stats_updated = now;
        }

        if (last_sq != fsm_input.sq) {
            last_sq = fsm_input.sq;
            usart_debug("In SQ %d\r\n", last_sq);
        }
        if (last_qrp != fsm_input.qrp) {
            last_qrp = fsm_input.qrp;
            usart_debug("In QRP %d\r\n", last_qrp);
        }
        if (last_discrim_d != fsm_input.discrim_d) {
            last_discrim_d = fsm_input.discrim_d;
            usart_debug("In D %d\r\n", last_discrim_d);
        }
        if (last_discrim_u != fsm_input.discrim_u) {
            last_discrim_u = fsm_input.discrim_u;
            usart_debug("In U %d\r\n", last_discrim_u);
        }
        if (last_wind_generator_ok != fsm_input.wind_generator_ok) {
            last_wind_generator_ok = fsm_input.wind_generator_ok;
            stats_wind_generator_moved();
            usart_debug("In eolienne %s\r\n", last_wind_generator_ok ? "vent" : "replie");
        }

        const int cw_psk_done = !cw_psk_busy();
        const int cw_done = cw_psk_done && only_zero_in_audio_buffer;

        // Set the done flag to 1 only once, when cw_done switches from 0 to 1
        if (last_cw_done != cw_done) {
            usart_debug("In cw_done change %d %d\r\n", cw_done, only_zero_in_audio_buffer);

            if (cw_done) {
                fsm_input.cw_psk_done = cw_done;
                leds_turn_off(LED_ORANGE);
            }

            last_cw_done = cw_done;
        }
        else {
            fsm_input.cw_psk_done = 0;
        }


        const int current_tone_1750_status = tone_1750_status();
#ifdef SIMULATOR
        gui_in_tone_1750 = current_tone_1750_status;
#endif
        fsm_input.det_1750 = current_tone_1750_status;
        pio_set_det_1750(current_tone_1750_status);

        fsm_input.long_1750 = tone_1750_for_5_seconds();

        // TODO implement a DTMF controlled state machine for setting SQ2
        pio_set_sq2(0);

        fsm_input.fax_mode = tone_fax_status();
        fsm_input.swr_high = swr_error_flag;
        fsm_input.hour_is_even = hour_is_even;

        struct tm time = {0};
        int time_valid = local_time(&time);
        if (!time_valid) {
            time_valid = local_derived_time(&time);
        }
        if (time_valid) {
            fsm_input.send_stats = (time.tm_hour == 22) ? 1 : 0;
        }

        fsm_update_inputs(&fsm_input);

        if (tm_trigger_button == 1 && last_tm_trigger_button == 0) {
            fsm_update_inputs(&fsm_input);
            fsm_balise_force();
        }
        last_tm_trigger_button = tm_trigger_button;

        fsm_update();
        fsm_balise_update();
        const int disable_1750_filter = fsm_sstv_update();
        pio_set_fax(disable_1750_filter);

        struct fsm_output_signals_t fsm_out;
        fsm_get_outputs(&fsm_out);

        pio_set_tx(fsm_out.tx_on);
        if (fsm_out.tx_on != last_tx_on) {
            stats_tx_switched(fsm_out.tx_on);
            last_tx_on = fsm_out.tx_on;
        }
        pio_set_mod_off(!fsm_out.modulation);

        // Add message to CW generator only on rising edge of trigger
        if (fsm_out.cw_psk_trigger && !cw_last_trigger && fsm_out.msg != NULL) {
            const int success = cw_psk_push_message(fsm_out.msg, fsm_out.cw_dit_duration, fsm_out.msg_frequency);
            if (!success) {
                usart_debug_puts("cw_psk_push_message failed");
            }

            leds_turn_on(LED_ORANGE);
        }
        cw_last_trigger = fsm_out.cw_psk_trigger;
    }
}

const int BLUE_LED_INTVL = 4;
static int blue_led_phase = 0;
static void nf_analyse(void __attribute__ ((unused))*pvParameters)
{
    while (1) {
        if (blue_led_phase == 0) {
            leds_turn_on(LED_BLUE);
        }
        else if (blue_led_phase == BLUE_LED_INTVL) {
            leds_turn_off(LED_BLUE);
        }

        blue_led_phase++;

        if (blue_led_phase >= BLUE_LED_INTVL * 2) {
            blue_led_phase = 0;
        }

        tone_do_analysis();
    }
}

static char ccounter_msg[MAX_CCOUNTER_SENTENCE_LEN];
static void read_in_coulomb_counter(void __attribute__ ((unused))*pvParameters)
{
    while (1) {
        int ok = usart_get_ccounter_msg(ccounter_msg); // times out after 2s
        if (ok) {
            size_t len = strlen(ccounter_msg);
            /* Ignore if \n follows \r or not, as that should never happen, and in any case
             * we don't want to send the \r or whatever could come after.
             */
            if (len > 2 && ccounter_msg[len-2] == '\r') {
                ccounter_msg[len-2] = '\0';
            }

            batterycharge_push_message(ccounter_msg);
            usart_debug_puts_header("CC: ", ccounter_msg);
        }
    }
}

#if configGENERATE_RUN_TIME_STATS
#include "stm32f4xx_conf.h"
#include "stm32f4xx_tim.h"

void vConfigureTimerForRunTimeStats()
{
    TIM_TimeBaseInitTypeDef SetupTimer;
    /* Enable timer 2, using the Reset and Clock Control register */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    SetupTimer.TIM_Prescaler = 0x0000;
    SetupTimer.TIM_CounterMode = TIM_CounterMode_Up;
    SetupTimer.TIM_Period = 0xFFFFFFFF;
    SetupTimer.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &SetupTimer);
    TIM_Cmd(TIM2, ENABLE); /* start counting by enabling CEN in CR1 */
}

unsigned long vGetTimerForRunTimeStats( void ) {
    return TIM_GetCounter(TIM2);
}

static TaskStatus_t taskstats[12];
static void print_task_stats(void) {
    uint32_t total_time;
    int n_tasks = uxTaskGetSystemState(taskstats, 12, &total_time);
    total_time /= 100UL;
    for (int t = 0; t < n_tasks; t++) {
        char status_indicator;
        switch(taskstats[t].eCurrentState )
        {
            case eReady:      status_indicator = 'R'; break;
            case eBlocked:    status_indicator = 'B'; break;
            case eSuspended:  status_indicator = 'S'; break;
            case eDeleted:    status_indicator = 'D'; break;
            case eRunning:    status_indicator = 'R'; break;
        }

        uint32_t task_time_percent = 0;
        if (total_time > 0) {
            task_time_percent = taskstats[t].ulRunTimeCounter / total_time;
        }

        usart_debug("TASK %d %s %c [%d] %d\r\n",
                taskstats[t].xTaskNumber,
                taskstats[t].pcTaskName,
                status_indicator,
                taskstats[t].usStackHighWaterMark,
                task_time_percent);
    }
}
#else
static void print_task_stats(void) {}
#endif
