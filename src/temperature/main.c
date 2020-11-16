#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "stm32f4xx_conf.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

#include "temperature.h"
#include "i2c.h"
#include "bmp085.h"

#include "debug.h"


ADC_InitTypeDef ADC_InitStruct;

ADC_CommonInitTypeDef ADC_CommonInitStruct;


// Private variables
volatile uint32_t time_var1, time_var2;

// Private function prototypes
void init();

// Tasks
static void detect_button_press(void *pvParameters);


void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                    signed char *pcTaskName )
{
    while (1) {};
}
extern void initialise_monitor_handles(void);

// Launcher task is here to make sure the scheduler is
// already running when calling the init functions.
static void launcher_task(void __attribute__ ((unused))*pvParameters)
{
    init();

    xTaskCreate(
            detect_button_press,
            "TaskButton",
            4*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        GPIO_SetBits(GPIOD, GPIO_Pin_13);
        temperature_update();
        GPIO_ResetBits(GPIOD, GPIO_Pin_13);
        taskYIELD();
    }
}

int main(void) {
    TaskHandle_t task_handle;
    xTaskCreate(
            launcher_task,
            "Launcher",
            2*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            &task_handle);

    /* Start the RTOS Scheduler */
    vTaskStartScheduler();

    /* HALT */
    while(1);
}

static void detect_button_press(void *pvParameters)
{
    GPIO_SetBits(GPIOD, GPIO_Pin_12);

    while (1) {
        if (GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0)>0) {

            while (GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0) > 0) {
                vTaskDelay(pdMS_TO_TICKS(100)); /* Button Debounce Delay */
            }

            GPIO_ResetBits(GPIOD, GPIO_Pin_12);
            GPIO_SetBits(GPIOD, GPIO_Pin_15);
            debug_print("Le jeu. Temp is: ");

            char t[32];
            // For debugging purposes only. snprinf has issues with %f
            snprintf(t, 32, "%d.%02d", (int)temperature_get(), (int)(temperature_get() * 100.0 - (int)(temperature_get()) * 100.0));
            debug_print(t);

            int32_t pressure = 0;
            int32_t temperature = 0;
            int success = bmp085_get_temp_pressure(&temperature, &pressure);
            snprintf(t, 32, " (%d) P=%ld, T=%ld", success, pressure, temperature);
            debug_print(t);
            debug_print("\n");

            GPIO_ResetBits(GPIOD, GPIO_Pin_15);
            GPIO_SetBits(GPIOD, GPIO_Pin_12);

            while (GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0) == 0) {
                vTaskDelay(pdMS_TO_TICKS(100)); /* Button Debounce Delay */
            }
        }
        taskYIELD();
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


    // Temperature using ADC
    
    // Enable ADC clock
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    // Init ADC
    ADC_CommonInitStruct.ADC_Mode = ADC_Mode_Independent;
    ADC_CommonInitStruct.ADC_Prescaler = ADC_Prescaler_Div8;
    ADC_CommonInitStruct.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
    ADC_CommonInitStruct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&ADC_CommonInitStruct);

    ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStruct.ADC_ScanConvMode = DISABLE;
    ADC_InitStruct.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStruct.ADC_NbrOfConversion = 1;
    ADC_Init(ADC1, &ADC_InitStruct);

    // Configure ADC1 to use temperture sensor
    ADC_RegularChannelConfig(ADC1, ADC_Channel_TempSensor, 1, ADC_SampleTime_144Cycles);

    // Enable temperature sensor
    ADC_TempSensorVrefintCmd(ENABLE);

    // Enable ADC
    ADC_Cmd(ADC1, ENABLE);

    debug_print("Init ADC done\n");

    // Enable I2C
    i2c_init();
    if (! bmp085_init() ) {
        debug_print("Init BMP085 fail\n");
    }
    else {
        debug_print("Init BMP085 ok\n");
    }
}

