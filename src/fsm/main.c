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
#include "fsm.h"
#include "common.h"

// Private variables
volatile uint32_t time_var1, time_var2;

static int tm_trigger = 0;


// Private function prototypes
void init();

// Tasks
static void detect_button_press(void *pvParameters);
static void exercise_fsm(void *pvParameters);
static void audio_task(void *pvParameters);

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

    cw_init(16000);

    pio_init();

    common_init();

    InitializeAudio(Audio16000HzSettings);
    SetAudioVolume(164);

    xTaskCreate(
            detect_button_press,
            "TaskButton",
            4*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            NULL);

    xTaskCreate(
            exercise_fsm,
            "TaskFSM",
            4*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            NULL);

    xTaskCreate(
            audio_task,
            "TaskAudio",
            configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            NULL);

    /* Start the RTOS Scheduler */
    vTaskStartScheduler();

    /* HALT */
    while(1);
}

// disabled
static void detect_button_press(void *pvParameters)
{
    GPIO_SetBits(GPIOD, GPIO_Pin_12);

    while (1) {
        if (GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0)>0) {

            while (GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0) > 0) {
                vTaskDelay(100 / portTICK_RATE_MS); /* Button Debounce Delay */
            }

            tm_trigger = 1;
            GPIO_SetBits(GPIOD, GPIO_Pin_12);

            while (GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0) == 0) {
                vTaskDelay(100 / portTICK_RATE_MS); /* Button Debounce Delay */
            }

            tm_trigger = 0;
            GPIO_ResetBits(GPIOD, GPIO_Pin_12);
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
        GPIO_ResetBits(GPIOD, GPIO_Pin_14);
        select_buffer = 1;
    } else {
        samples = audio_buffer1;
        GPIO_SetBits(GPIOD, GPIO_Pin_14);
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

static void audio_task(void *pvParameters)
{
    PlayAudioWithCallback(audio_callback, NULL);

    while (1) {
        taskYIELD();
    }
}

static struct fsm_input_signals_t fsm_input;
static void exercise_fsm(void *pvParameters)
{
    int cw_last_trigger = 0;

    fsm_input.humidity = 0;
    fsm_input.temp = 15;
    fsm_input.swr_high = 0;
    fsm_input.sstv_mode = 0;
    fsm_input.wind_generator_ok = 1;
    while (1) {
        pio_set_fsm_signals(&fsm_input);
        fsm_input.start_tm = tm_trigger; // user button
        if (fsm_input.start_tm) {
            GPIO_SetBits(GPIOD, GPIO_Pin_15);
        }
        else {
            GPIO_ResetBits(GPIOD, GPIO_Pin_15);
        }
        fsm_input.sq = fsm_input.carrier; // TODO clarify
        fsm_input.cw_done = !cw_busy();

        if (fsm_input.cw_done) {
            GPIO_ResetBits(GPIOD, GPIO_Pin_13);
        }
        else {
            GPIO_SetBits(GPIOD, GPIO_Pin_13);
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
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    // Configure PD12, PD13, PD14 and PD15 in output pushpull mode
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13| GPIO_Pin_14| GPIO_Pin_15;
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
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

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

