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

#include "Core/fsm.h"
#include "GPIO/usart.h"
#include <time.h>

extern const char * gui_last_fsm_states[];
extern int gui_last_fsm_states_timestamps[];

void fsm_state_switched(const char *new_state) {
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
