#include "../../../common/src/GPIO/temperature.c"

extern int gui_temperature_valid;
extern float gui_temperature;

static void temperature_task(void *pvParameters) {

    while (1) {

        _temperature_last_value = gui_temperature;
        _temperature_valid = gui_temperature_valid;

        vTaskDelay(1000 / portTICK_PERIOD_MS);

    }
}