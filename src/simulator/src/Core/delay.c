#include "Core/delay.h"
#include <unistd.h>

void delay_init(void) {
}

void delay_us(int micros) {
    usleep(micros);
}

void delay_ms(int millis) {
    sleep(millis);
}

