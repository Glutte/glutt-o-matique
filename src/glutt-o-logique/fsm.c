#include "../common/src/Core/fsm.c"


void fsm_state_switched(char * new_state) {
    usart_debug_puts("FSM: ");
    usart_debug_puts(new_state);
    usart_debug_puts("\r\n");
}
