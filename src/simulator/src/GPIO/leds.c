#include "GPIO/leds.h"


extern char led_blue;
extern char led_green;
extern char led_orange;
extern char led_red;

void leds_turn_on(int l) {

    switch (l) {
        case LED_GREEN:
            led_green = 1;
            break;
        case LED_ORANGE:
            led_orange = 1;
            break;
        case LED_RED:
            led_red = 1;
            break;
        case LED_BLUE:
            led_blue = 1;
            break;

    }
}

void leds_turn_off(int l) {

    switch (l) {
        case LED_GREEN:
            led_green = 0;
            break;
        case LED_ORANGE:
            led_orange = 0;
            break;
        case LED_RED:
            led_red = 0;
            break;
        case LED_BLUE:
            led_blue = 0;
            break;
    }
}
