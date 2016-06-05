#include "GPIO/usart.h"
#include "src/GPS/gps_sim.h"

void gps_usart_send(char * string) {

    while(*string) {
        usart_gps_process_char(*string);

        string++;
    }

}
