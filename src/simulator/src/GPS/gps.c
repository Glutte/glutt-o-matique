#include "../../../common/src/GPS/gps.c"

void gps_usart_send(char * string) {

    while(*string) {
        usart_gps_process_char(*string);

        string++;
    }

}
