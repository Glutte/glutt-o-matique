#include "stm32f4xx_conf.h"

#include "temperature.h"

float temperature_value = 0;

void temperature_update() {
    ADC_SoftwareStartConv(ADC1); //Start the conversion

    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);

    temperature_value = ((ADC_GetConversionValue(ADC1) * TEMP_V_BOARD / 0xFFF) - TEMP_V25) / TEMP_AVG_SLOPE + 25.0;
}

int temperature_get(float *temp) {
    *temp = temperature_value;
    return 1;
}
