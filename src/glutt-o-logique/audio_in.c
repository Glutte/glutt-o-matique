/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Matthias P. Braendli, Maximilien Cuony
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
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_adc.h"

#include "Core/common.h"
#include "Audio/audio_in.h"
#include "queue.h"

// APB1 prescaler = 4, see bsp/system_stm32f4xx.c
#define APB1_FREQ (168000000ul / 4)
#define ADC2_SAMPLE_FREQ 16000
#define ADC2_CHANNEL_AUDIO ADC_Channel_14

// see doc/pio.txt for allocation
#define PINS_ADC2 /* PB1 on ADC2 IN9 */ (GPIO_Pin_1)

#warning "TODO: initialise ADC2 and use it for NF input"

// The TIM6 ISR reads from ADC2 and writes into this buffer
static int16_t adc2_values[AUDIO_IN_BUF_LEN];
static int     adc2_values_end = 0;

// ADC2 data from interrupt to userspace goes through the queue
static QueueHandle_t adc2_values_queue;

/* ISR for Timer6 and DAC1&2 underrun */
void TIM6_DAC_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM6, TIM_IT_Update)) {
        if (ADC_GetFlagStatus(ADC2, ADC_FLAG_EOC) == SET) {
            uint16_t value = ADC_GetConversionValue(ADC2);
            /* input range: 0 to 65535
             * output range: -32768 to 32767 */
            adc2_values[adc2_values_end++] = (int32_t)value - 32768;
            if (adc2_values_end == AUDIO_IN_BUF_LEN) {
                int success = xQueueSendToBackFromISR(
                        adc2_values_queue,
                        adc2_values,
                        NULL);

                adc2_values_end = 0;

                if (success == pdFALSE) {
                    trigger_fault(FAULT_SOURCE_ADC2);
                }
            }
        }
        else {
#warning "handle fault"
        }

        ADC_SoftwareStartConv(ADC2);

        TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
    }
}

// Timer6 is used for ADC2 sampling
static void enable_timer6(void)
{
    /* TIM6 Periph clock enable */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);

    /* Time base configuration */
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = (int)(APB1_FREQ/ADC2_SAMPLE_FREQ);
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM6, &TIM_TimeBaseStructure);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM6_DAC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM6, ENABLE);
    ADC_SoftwareStartConv(ADC2);
    TIM_ITConfig(TIM6, TIM_IT_Update, ENABLE);
}


void audio_in_initialize(int rate)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_Pin   = PINS_ADC2;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // Init ADC2 for NF input
    ADC_InitTypeDef ADC_InitStruct;
    ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStruct.ADC_ScanConvMode = DISABLE;
    ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStruct.ADC_NbrOfConversion = 1;
    ADC_Init(ADC2, &ADC_InitStruct);

    ADC_Cmd(ADC2, ENABLE);

    adc2_values_queue = xQueueCreate(4, AUDIO_IN_BUF_LEN);
    if (adc2_values_queue == 0) {
        while(1); /* fatal error */
    }

    enable_timer6();
}

void audio_in_get_buffer(int16_t *buffer /*of length AUDIO_IN_BUF_LEN*/ )
{
    while (!xQueueReceive(adc2_values_queue, buffer, portMAX_DELAY)) {}
}
