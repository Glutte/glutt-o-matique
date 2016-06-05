#include "../common/src/GPIO/leds.c"

#include "stm32f4xx_conf.h"

#include "leds.h"

void leds_turn_on(int l) {

    switch (l) {
        case LED_GREEN:
            GPIO_SetBits(GPIOD, GPIOD_BOARD_LED_GREEN);
            break;
        case LED_ORANGE:
            GPIO_SetBits(GPIOD, GPIOD_BOARD_LED_ORANGE);
            break;
        case LED_RED:
            GPIO_SetBits(GPIOD, GPIOD_BOARD_LED_RED);
            break;
        case LED_BLUE:
            GPIO_SetBits(GPIOD, GPIOD_BOARD_LED_BLUE);
            break;

    }
}

void leds_turn_off(int l) {

    switch (l) {
        case LED_GREEN:
            GPIO_ResetBits(GPIOD, GPIOD_BOARD_LED_GREEN);
            break;
        case LED_ORANGE:
            GPIO_ResetBits(GPIOD, GPIOD_BOARD_LED_ORANGE);
            break;
        case LED_RED:
            GPIO_ResetBits(GPIOD, GPIOD_BOARD_LED_RED);
            break;
        case LED_BLUE:
            GPIO_ResetBits(GPIOD, GPIOD_BOARD_LED_BLUE);
            break;
    }
}
