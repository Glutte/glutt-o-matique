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

#include "analog_input.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "Core/delay.h"
#include "Core/common.h"
#include "stm32f4xx_adc.h"
#include <math.h>
#include "GPIO/usart.h"

// Measured on the board itself
const float v_ref = 2.965f;

void analog_init(void)
{
    // Enable ADC and GPIOA clocks
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    // Set analog input pins mode
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_Pin   = PINS_ANALOG;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Init ADC1 for supply measurement
    ADC_CommonInitTypeDef ADC_CommonInitStruct;

    ADC_CommonInitStruct.ADC_Mode = ADC_Mode_Independent;
    ADC_CommonInitStruct.ADC_Prescaler = ADC_Prescaler_Div8;
    ADC_CommonInitStruct.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
    ADC_CommonInitStruct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&ADC_CommonInitStruct);

    ADC_InitTypeDef ADC_InitStruct;
    ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStruct.ADC_ScanConvMode = DISABLE;
    ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStruct.ADC_NbrOfConversion = 3;
    ADC_Init(ADC1, &ADC_InitStruct);

    // Enable ADC
    ADC_Cmd(ADC1, ENABLE);
}

static uint16_t analog_read_channel(uint8_t channel)
{
    ADC_RegularChannelConfig(ADC1,
            channel,
            1,
            ADC_SampleTime_480Cycles);

    ADC_SoftwareStartConv(ADC1);

    /* Timeout:
     * System clock is at 168MHz, ADC is on APB2 which has a prescaler of 2.
     * 480 cycles at 84Mhz is about 6us.
     *
     * If we have no result after 1000ms it is a real problem. Keep in mind
     * we could get preempted.
     */

    int ready = 0;

    vTaskSuspendAll();
    for (int i = 0; i < 10000; i++) {
        if (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == SET) {
            ready = 1;
            break;
        }
        delay_us(100);
    }
    xTaskResumeAll();

    if (!ready) {
        trigger_fault(FAULT_SOURCE_ADC1);
    }

    return ADC_GetConversionValue(ADC1);
}

float analog_measure_12v(void)
{
    const uint16_t raw_value = analog_read_channel(ADC_CHANNEL_SUPPLY);

    const float adc_max_value = (1 << 12);

    // Convert ADC measurement to voltage
    float voltage = ((float)raw_value*v_ref/adc_max_value);

    // Compensate resistor divider on board (see schematic)
    return voltage * 202.0f / 22.0f;
}


int analog_measure_swr(int *forward_mv, int* reflected_mv)
{
    const uint16_t raw_swr_fwd_value = analog_read_channel(ADC_CHANNEL_SWR_FWD);
    const uint16_t raw_swr_refl_value = analog_read_channel(ADC_CHANNEL_SWR_REFL);

    const float adc_max_value = (1 << 12);

    // Convert ADC measurement to mV (includes times 100 amplifier)
    const int swr_fwd = ((float)raw_swr_fwd_value*10.0f*v_ref/adc_max_value);
    const int swr_refl = ((float)raw_swr_refl_value*10.0f*v_ref/adc_max_value);

    *forward_mv = swr_fwd;
    *reflected_mv = swr_refl;

    return 1;
}

