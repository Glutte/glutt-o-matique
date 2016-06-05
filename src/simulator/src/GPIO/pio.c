#include "Core/fsm.h"

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
