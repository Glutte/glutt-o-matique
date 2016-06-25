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
#include "GPIO/pio.h"

extern char gui_out_tx;
extern char gui_out_mod_off;
extern char gui_out_qrp;

extern int gui_in_button;
extern int gui_in_qrp_n;
extern int gui_in_1750_n;
extern int gui_in_sq_n;
extern int gui_in_u;
extern int gui_in_d;
extern int gui_in_replie_n;
extern int gui_in_fax_n;
extern char led_gps;

void pio_init(void) {
}

void pio_set_tx(int on) {
    gui_out_tx = on;
}

void pio_set_mod_off(int mod_off) {
    gui_out_mod_off = mod_off;
}

void pio_set_qrp(int on) {
    gui_out_qrp = on;
}

void pio_set_gps_epps(int on) {
    led_gps = on;
}

void pio_set_fsm_signals(struct fsm_input_signals_t* sig) {

    sig->qrp = gui_in_qrp_n ? 0 : 1;
    sig->tone_1750 = gui_in_1750_n ? 0 : 1;
    sig->sq = gui_in_sq_n ? 0 : 1;
    sig->discrim_u = gui_in_u ? 1 : 0;
    sig->discrim_d = gui_in_d ? 1 : 0;
    sig->wind_generator_ok = gui_in_replie_n ? 1 : 0;
    sig->sstv_mode = gui_in_fax_n ? 0 : 1;

}

int pio_read_button() {
    return gui_in_button ? 1 : 0;
}
