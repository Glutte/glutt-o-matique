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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "stm32f4xx_conf.h"
#include "audio.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "cw.h"
#include "pio.h"
#include "i2c.h"
#include "gps.h"
#include "fsm.h"
#include "common.h"

#define GPIOD_BOARD_LED_GREEN  GPIO_Pin_12
#define GPIOD_BOARD_LED_ORANGE GPIO_Pin_13
#define GPIOD_BOARD_LED_RED    GPIO_Pin_14
#define GPIOD_BOARD_LED_BLUE   GPIO_Pin_15

// Private variables
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

struct cw_msg_s {
    const char* msg;
    int freq;
    int dit_duration;
};

void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                    signed char *pcTaskName )
{
    while (1) {};
}

int main(void) {
    init();

    TaskHandle_t task_handle;
    xTaskCreate(
            launcher_task,
            "TaskLauncher",
            configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            &task_handle);

    if (!task_handle) {
        trigger_fault(FAULT_SOURCE_MAIN);
    }

    /* Start the RTOS Scheduler */
    vTaskStartScheduler();

    /* HALT */
    while(1);
}

// Launcher task is here to make sure the scheduler is
// already running when calling the init functions.
static void launcher_task(void *pvParameters)
{
    cw_init(16000);
    pio_init();
    i2c_init();
    common_init();
    gps_init();

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

    InitializeAudio(Audio16000HzSettings);
    SetAudioVolume(210);

    PlayAudioWithCallback(audio_callback, NULL);


    /* We are done now, suspend this task
     * With FreeDOS' heap_1.c, we cannot delete it.
     * See freertos.org -> More Advanced... -> Memory Management
     * for more info.
     */
    while (1) {
        vTaskSuspend(NULL);
    }
}

static void detect_button_press(void *pvParameters)
{
    GPIO_SetBits(GPIOD, GPIOD_BOARD_LED_GREEN);

    while (1) {
        if (GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0)>0) {

            while (GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0) > 0) {
                vTaskDelay(100 / portTICK_RATE_MS); /* Button Debounce Delay */
            }

            tm_trigger = 1;
            GPIO_SetBits(GPIOD, GPIOD_BOARD_LED_GREEN);

            while (GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0) == 0) {
                vTaskDelay(100 / portTICK_RATE_MS); /* Button Debounce Delay */
            }

            tm_trigger = 0;
            GPIO_ResetBits(GPIOD, GPIOD_BOARD_LED_GREEN);
        }
        taskYIELD();
    }
}

static void audio_callback(void* context, int select_buffer)
{
    static int16_t audio_buffer0[AUDIO_BUF_LEN];
    static int16_t audio_buffer1[AUDIO_BUF_LEN];
    int16_t *samples;

    if (select_buffer == 0) {
        samples = audio_buffer0;
        GPIO_ResetBits(GPIOD, GPIOD_BOARD_LED_RED);
        select_buffer = 1;
    } else {
        samples = audio_buffer1;
        GPIO_SetBits(GPIOD, GPIOD_BOARD_LED_RED);
        select_buffer = 0;
    }

    size_t samples_len = cw_fill_buffer(samples, AUDIO_BUF_LEN);

    if (samples_len == 0) {
        for (int i = 0; i < AUDIO_BUF_LEN; i++) {
            samples[i] = 0;
        }

        samples_len = AUDIO_BUF_LEN;
    }

    ProvideAudioBufferWithoutBlocking(samples, samples_len);
}


static struct gps_time_s gps_time;
static void gps_monit_task(void *pvParameters)
{
    GPIO_SetBits(GPIOD, GPIOD_BOARD_LED_BLUE);

    while (1) {
        if (gps_locked()) {

            gps_utctime(&gps_time);

            if (gps_time.sec % 4 >= 2) {
                GPIO_SetBits(GPIOD, GPIOD_BOARD_LED_BLUE);
            }
            else {
                GPIO_ResetBits(GPIOD, GPIOD_BOARD_LED_BLUE);
            }
        }
        taskYIELD();
    }
}

static struct fsm_input_signals_t fsm_input;
static void exercise_fsm(void *pvParameters)
{
    int cw_last_trigger = 0;
    int last_tm_trigger = 0;

    fsm_input.humidity = 0;
    fsm_input.temp = 15;
    fsm_input.swr_high = 0;
    fsm_input.sstv_mode = 0;
    fsm_input.wind_generator_ok = 1;
    while (1) {
        pio_set_fsm_signals(&fsm_input);
        fsm_input.start_tm = (tm_trigger == 1 && last_tm_trigger == 0) ? 1 : 0;
        last_tm_trigger = tm_trigger;

        fsm_input.sq = fsm_input.carrier; // TODO clarify
        fsm_input.cw_done = !cw_busy();

        if (fsm_input.cw_done) {
            GPIO_ResetBits(GPIOD, GPIOD_BOARD_LED_ORANGE);
        }
        else {
            GPIO_SetBits(GPIOD, GPIOD_BOARD_LED_ORANGE);
        }

        fsm_update_inputs(&fsm_input);
        fsm_update();

        struct fsm_output_signals_t fsm_out;
        fsm_get_outputs(&fsm_out);

        pio_set_led_red(fsm_out.tx_on);
        pio_set_led_yel(fsm_out.modulation);
        pio_set_led_grn(fsm_input.carrier);

        // Add message to CW generator only on rising edge of trigger
        if (fsm_out.cw_trigger && !cw_last_trigger) {
            cw_push_message(fsm_out.cw_msg, 140 /*fsm_out.cw_speed*/, fsm_out.cw_frequency);
        }

        cw_last_trigger = fsm_out.cw_trigger;
    }
}


void init() {
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    // ---------- SysTick timer -------- //
    if (SysTick_Config(SystemCoreClock / 1000)) {
        // Capture error
        while (1){};
    }

    // Enable full access to FPU (Should be done automatically in system_stm32f4xx.c):
    //SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  // set CP10 and CP11 Full Access

    // GPIOD Periph clock enable
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    // Configure PD12, PD13, PD14 and PD15 in output pushpull mode
    GPIO_InitStructure.GPIO_Pin =
        GPIOD_BOARD_LED_GREEN |
        GPIOD_BOARD_LED_ORANGE |
        GPIOD_BOARD_LED_RED |
        GPIOD_BOARD_LED_BLUE;

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // Init PushButton
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);


    // ------ UART ------ //

    // Clock
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    // IO
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource6, GPIO_AF_USART1);

    // Conf
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &USART_InitStructure);

    // Enable
    USART_Cmd(USART2, ENABLE);
}

