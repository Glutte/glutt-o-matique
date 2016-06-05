#include "../../../common/src/Core/fsm.c"


extern char * gui_last_fsm_states[];
extern int * gui_last_fsm_states_timestamps[];


void fsm_state_switched(char * new_state) {
    usart_debug_puts_header("FSM: ", new_state);

    for (int i = 8; i >= 0; i--) {
        gui_last_fsm_states[i + 1] = gui_last_fsm_states[i];
        gui_last_fsm_states_timestamps[i + 1] = gui_last_fsm_states_timestamps[i];
    }


    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    gui_last_fsm_states[0] = new_state;
    gui_last_fsm_states_timestamps[0] = t->tm_hour * 10000 + t->tm_min * 100 + t->tm_sec;
}
