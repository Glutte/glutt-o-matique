/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Matthias P. Braendli
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

#include <string.h>
#include "common.h"
#include "fsm.h"

static struct fsm_input_signals_t fsm_in;
static struct fsm_output_signals_t fsm_out;

static fsm_state_t current_state;

// Keep track of when we last entered a given state, measured
// in ms using the timestamp_now() function
static uint64_t timestamp_state[_NUM_FSM_STATES];

void fsm_init() {
    memset(&fsm_in, 0, sizeof(fsm_in));
    memset(&fsm_out, 0, sizeof(fsm_out));

    memset(timestamp_state, 0, _NUM_FSM_STATES * sizeof(*timestamp_state));

    current_state = FSM_OISIF;
}

// Calculate the time spent in the current state
uint64_t fsm_current_state_time_ms(void) {
    return timestamp_now() - timestamp_state[current_state];
}

uint64_t fsm_current_state_time_s(void) {
    return fsm_current_state_time_ms() / 1000;
}

// Between turns in a QSO, the repeater sends a letter in CW,
// different messages are possible. They are sorted here from
// low to high priority.
const char* letter_all_ok    = "k";
const char* letter_sstv      = "s";
const char* letter_qrp       = "g";
const char* letter_freq_high = "u";
const char* letter_freq_low  = "d";
const char* letter_swr_high  = "r";

const char* fsm_select_letter(void) {
    if (fsm_in.swr_high) {
        return letter_swr_high;
    }
    else if (fsm_in.discrim_d) {
        return letter_freq_low;
    }
    else if (fsm_in.discrim_u) {
        return letter_freq_high;
    }
    else if (fsm_in.qrp) {
        return letter_qrp;
    }
    else if (fsm_in.sstv_mode) {
        return letter_sstv;
    }

    return letter_all_ok;
}

void fsm_update() {

    fsm_state_t next_state = current_state;

    // Some defaults for the outgoing signals
    fsm_out.tx_on = 0;
    fsm_out.modulation = 0;
    fsm_out.cw_trigger = 0;
    // other output signals keep their value

    switch (current_state) {
        case FSM_OISIF:
            if (fsm_in.tone_1750 && fsm_in.sq) {
                next_state = FSM_OPEN1;
            }
            else if (fsm_in.start_tm) {
                if (fsm_in.qrp || fsm_in.swr_high) {
                    next_state = FSM_BALISE_SPECIALE;
                }
                else {
                    next_state = FSM_BALISE_LONGUE;
                }
            }
            else if (!fsm_in.qrp && fsm_current_state_time_s() > 20 * 60) {
                next_state = FSM_BALISE_COURTE;
            }
            break;

        case FSM_OPEN1:
            fsm_out.tx_on = 1;

            if (!fsm_in.sq) {
                next_state = FSM_OPEN2;
            }
            break;

        case FSM_OPEN2:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;

            if (fsm_current_state_time_ms() > 200) {
                next_state = FSM_LETTRE;
            }
            break;

        case FSM_LETTRE:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;
            fsm_out.cw_msg = fsm_select_letter();
            fsm_out.cw_trigger = 1;

            if (fsm_in.cw_done) {
                next_state = FSM_ECOUTE;
            }
            break;

        case FSM_ECOUTE:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;

            if (fsm_in.sq) {
                next_state = FSM_QSO;
            }
            else if (fsm_current_state_time_s() > 6 &&
                    timestamp_state[FSM_ECOUTE] - timestamp_state[FSM_OPEN2] <
                        1000ul * 5) {
                next_state = FSM_ATTENTE;
            }
            else if (fsm_current_state_time_s() > 5 &&
                    timestamp_state[FSM_ECOUTE] - timestamp_state[FSM_OPEN2] <
                        1000ul * 5 * 60) {
                next_state = FSM_OISIF;
            }
            else if (fsm_current_state_time_s() > 5 &&
                    timestamp_state[FSM_ECOUTE] - timestamp_state[FSM_OPEN2] <
                        1000ul * 10 * 60) {
                next_state = FSM_TEXTE_73;
            }
            else if (fsm_current_state_time_s() > 5 &&
                    timestamp_state[FSM_ECOUTE] - timestamp_state[FSM_OPEN2] <
                        1000ul * 15 * 60) {
                next_state = FSM_TEXTE_HB9G;
            }
            else if (fsm_current_state_time_s() > 5 &&
                    timestamp_state[FSM_ECOUTE] - timestamp_state[FSM_OPEN2] >=
                        1000ul * 15 * 60) {
                next_state = FSM_TEXTE_LONG;
            }
            break;

        case FSM_ATTENTE:
            if (fsm_in.sq) {
                next_state = FSM_ECOUTE;
            }
            else if (fsm_current_state_time_s() > 15) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_QSO:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;

            if (!fsm_in.sq) {
                next_state = FSM_LETTRE;
            }
            else if (fsm_current_state_time_s() > 5 * 60) {
                next_state = FSM_ANTI_BAVARD;
            }
            break;

        case FSM_ANTI_BAVARD:
            fsm_out.tx_on = 1;
            // No modulation!
            fsm_out.cw_msg = "hi hi";
            fsm_out.cw_trigger = 1;

            if (fsm_in.cw_done) {
                next_state = FSM_BLOQUE;
            }
            break;

        case FSM_BLOQUE:
            if (fsm_current_state_time_s() > 10) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_TEXTE_73:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;
            fsm_out.cw_msg = "73";
            fsm_out.cw_trigger = 1;

            if (fsm_in.sq) {
                next_state = FSM_QSO;
            }
            else if (fsm_in.cw_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_TEXTE_HB9G:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;
            fsm_out.cw_msg = "hb9g";
            fsm_out.cw_trigger = 1;

            if (fsm_in.sq) {
                next_state = FSM_QSO;
            }
            else if (fsm_in.cw_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_TEXTE_LONG:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;

            if (random_bool()) {
                fsm_out.cw_msg = "hb9g 1628m";
            }
            else {
                fsm_out.cw_msg = "hb9g jn36bk";
            }
            fsm_out.cw_trigger = 1;

            if (fsm_in.sq) {
                next_state = FSM_QSO;
            }
            else if (fsm_in.cw_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_BALISE_LONGUE:
            fsm_out.tx_on = 1;

            // TODO transmit humidity
            // TODO read voltage
            if (fsm_in.wind_generator_ok) {
                fsm_out.cw_msg = "hb9g jn36bk 1628m u 10v5 =  T 11  73";
                // = means same voltage as previous
                // + means higher
                // - means lower
            }
            else {
                fsm_out.cw_msg = "hb9g jn36bk 1628m u 10v5 =  T 11  #";
                // The # is the SK digraph
            }
            fsm_out.cw_trigger = 1;

            if (fsm_in.sq) {
                next_state = FSM_OPEN2;
            }
            else if (fsm_in.cw_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_BALISE_SPECIALE:
            fsm_out.tx_on = 1;
            // TODO read voltage
            if (fsm_in.wind_generator_ok) {
                fsm_out.cw_msg = "hb9g u 10v5 73";
            }
            else {
                fsm_out.cw_msg = "hb9g u 10v5 #"; // The # is the SK digraph
            }
            fsm_out.cw_trigger = 1;

            if (fsm_in.sq) {
                next_state = FSM_OPEN2;
            }
            else if (fsm_in.cw_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_BALISE_COURTE:
            fsm_out.tx_on = 1;

            {
                int rand = random_bool() * 2 + random_bool();

                if (rand == 0) {
                    fsm_out.cw_msg = "hb9g";
                }
                else if (rand == 1) {
                    fsm_out.cw_msg = "hb9g jn36bk";
                }
                else if (rand == 2) {
                    fsm_out.cw_msg = "hb9g 1628m";
                }
                else {
                    fsm_out.cw_msg = "hb9g jn36bk 1628m";
                }
            }
            fsm_out.cw_trigger = 1;

            if (fsm_in.sq) {
                next_state = FSM_OPEN2;
            }
            else if (fsm_in.cw_done) {
                next_state = FSM_OISIF;
            }
            break;
        default:
        // Should never happen
        next_state = FSM_OISIF;
            break;
    }

    current_state = next_state;
    timestamp_state[next_state] = timestamp_now();
}

