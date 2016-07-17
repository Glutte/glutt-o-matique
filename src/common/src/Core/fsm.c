/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Matthias P. Braendli
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
#include <stdio.h>
#include <stdint.h>
#include "Core/common.h"
#include "Core/fsm.h"
#include "GPIO/usart.h"
#include "GPIO/temperature.h"
#include "GPIO/analog.h"

static struct fsm_input_signals_t fsm_in;
static struct fsm_output_signals_t fsm_out;

static fsm_state_t current_state;

// Keep track of when we last entered a given state, measured
// in ms using the timestamp_now() function
static uint64_t timestamp_state[_NUM_FSM_STATES];

static int last_supply_voltage_decivolts = 0;

#define CW_MESSAGE_BALISE_LEN 64
static char cw_message_balise[CW_MESSAGE_BALISE_LEN];


// Each 20 minutes, send a SHORT_BEACON
#define SHORT_BEACON_MAX (60 * 20)
// Reset the counter if the QSO was 10m too long
#define SHORT_BEACON_RESET_IF_QSO (60 * 10)

// The counter (up to 20 minutes) for the short balise
static int short_beacon_counter_s = 0;
static uint64_t short_beacon_counter_last_update = 0;

// The last start of the last qso
static uint64_t last_qso_start_timestamp = 0;


void fsm_init() {
    memset(&fsm_in, 0, sizeof(fsm_in));
    memset(&fsm_out, 0, sizeof(fsm_out));

    memset(timestamp_state, 0, _NUM_FSM_STATES * sizeof(*timestamp_state));
    timestamp_state[FSM_OISIF] = timestamp_now();

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
const char* letter_all_ok    = "K";
const char* letter_sstv      = "S";
const char* letter_qrp       = "G";
const char* letter_freq_high = "U";
const char* letter_freq_low  = "D";
const char* letter_swr_high  = "R";

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
    fsm_out.cw_psk31_trigger = 0;
    fsm_out.cw_dit_duration = 50;
    fsm_out.msg_frequency = 960;
    // other output signals keep their value

    // Clear the ack flag if the start_tm has been cleared
    if (!fsm_in.start_tm && fsm_out.ack_start_tm) {
        fsm_out.ack_start_tm = 0;
    }

    switch (current_state) {
        case FSM_OISIF:

            // Check the length of the last QSO, and reset the SHORT_BEACON counter if needed
            if (last_qso_start_timestamp != 0) {

                if ((timestamp_now() - last_qso_start_timestamp) > 1000 * SHORT_BEACON_RESET_IF_QSO) {
                    short_beacon_counter_s = 0;
                }

                last_qso_start_timestamp = 0;
            }

            // Increment the SHORT_BEACON counter based on time spent in the state
            if (short_beacon_counter_s < SHORT_BEACON_MAX) {
                while(short_beacon_counter_s < SHORT_BEACON_MAX && (fsm_current_state_time_s() - short_beacon_counter_last_update > 1)) {
                    short_beacon_counter_last_update++;
                    short_beacon_counter_s++;
                }
            }

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
            else if (!fsm_in.qrp && short_beacon_counter_s == SHORT_BEACON_MAX) {
                short_beacon_counter_s = 0;
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
            fsm_out.msg = fsm_select_letter();
            if (fsm_out.msg[0] == 'G') {
                // The letter 'G' is a bit different
                fsm_out.msg_frequency    = 696;
                fsm_out.cw_dit_duration = 70;
            }
            fsm_out.cw_psk31_trigger = 1;

            if (fsm_in.cw_psk31_done) {
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

            // Save the starting timestamp, if there is none
            if (last_qso_start_timestamp == 0) {
                last_qso_start_timestamp = timestamp_now();
            }

            if (!fsm_in.sq && fsm_current_state_time_s() < 5) {
                /* To avoid that very short open squelch triggers
                 * transmit CW letters all the time. Some people
                 * enjoy doing that.
                 */
                next_state = FSM_ECOUTE;
            }
            else if (!fsm_in.sq && fsm_current_state_time_s() >= 5) {
                next_state = FSM_LETTRE;
            }
            else if (fsm_current_state_time_s() > 5 * 60) {
                next_state = FSM_ANTI_BAVARD;
            }
            break;

        case FSM_ANTI_BAVARD:
            fsm_out.tx_on = 1;
            // No modulation!
            fsm_out.msg = " HI HI";
            fsm_out.cw_psk31_trigger = 1;

            if (fsm_in.cw_psk31_done) {
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
            fsm_out.msg_frequency    = 696;
            fsm_out.cw_dit_duration = 70;
            fsm_out.msg = " 73";
            fsm_out.cw_psk31_trigger = 1;

            if (fsm_in.sq) {
                next_state = FSM_QSO;
            }
            else if (fsm_in.cw_psk31_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_TEXTE_HB9G:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;
            fsm_out.msg_frequency   = 696;
            fsm_out.cw_dit_duration = 70;
            fsm_out.msg = " HB9G";
            fsm_out.cw_psk31_trigger = 1;

            if (fsm_in.sq) {
                next_state = FSM_QSO;
            }
            else if (fsm_in.cw_psk31_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_TEXTE_LONG:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;

            fsm_out.msg_frequency   = 696;
            fsm_out.cw_dit_duration = 70;

            if (random_bool()) {
                fsm_out.msg = " HB9G 1628M";
            }
            else {
                fsm_out.msg = " HB9G JN36BK";
            }
            fsm_out.cw_psk31_trigger = 1;

            if (fsm_in.sq) {
                next_state = FSM_QSO;
            }
            else if (fsm_in.cw_psk31_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_BALISE_LONGUE:
            fsm_out.tx_on = 1;
            fsm_out.msg_frequency   = 588;
            fsm_out.cw_dit_duration = 110;
            fsm_out.ack_start_tm    = 1;

            {
                const float supply_voltage = round_float_to_half_steps(analog_measure_12v());
                const int supply_decivolts = supply_voltage * 10.0f;

                char *eol_info = "73";
                if (!fsm_in.wind_generator_ok) {
                    eol_info = "\\";
                    // The backslash is the SK digraph
                }

                char supply_trend = '=';
                // = means same voltage as previous
                // + means higher
                // - means lower
                if (last_supply_voltage_decivolts < supply_decivolts) {
                    supply_trend = '+';
                }
                else if (last_supply_voltage_decivolts > supply_decivolts) {
                    supply_trend = '-';
                }

                if (temperature_valid()) {
                    snprintf(cw_message_balise, CW_MESSAGE_BALISE_LEN-1,
                            "  HB9G JN36BK 1628M U %dV%01d %c  T %d  %s",
                            supply_decivolts / 10,
                            supply_decivolts % 10,
                            supply_trend,
                            (int)(round_float_to_half_steps(temperature_get())),
                            eol_info);
                }
                else {
                    snprintf(cw_message_balise, CW_MESSAGE_BALISE_LEN-1,
                            "  HB9G JN36BK 1628M U %dV%01d %c  %s",
                            supply_decivolts / 10,
                            supply_decivolts % 10,
                            supply_trend,
                            eol_info);
                }

                fsm_out.msg = cw_message_balise;

                last_supply_voltage_decivolts = supply_decivolts;

                fsm_out.cw_psk31_trigger = 1;
            }

            if (fsm_in.cw_psk31_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_BALISE_SPECIALE:
            fsm_out.tx_on = 1;
            fsm_out.msg_frequency   = 696;
            fsm_out.cw_dit_duration = 70;
            fsm_out.ack_start_tm    = 1;

            {
                const float supply_voltage = round_float_to_half_steps(analog_measure_12v());
                const int supply_decivolts = supply_voltage * 10.0f;

                char *eol_info = "73";
                if (!fsm_in.wind_generator_ok) {
                    eol_info = "\\";
                    // The backslash is the SK digraph
                }

                snprintf(cw_message_balise, CW_MESSAGE_BALISE_LEN-1,
                        "  HB9G U %dV%01d %s",
                        supply_decivolts / 10,
                        supply_decivolts % 10,
                        eol_info);

                fsm_out.msg = cw_message_balise;

                fsm_out.cw_psk31_trigger = 1;
            }

            if (fsm_in.cw_psk31_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_BALISE_COURTE:
        case FSM_BALISE_COURTE_OPEN:

            fsm_out.tx_on = 1;

            fsm_out.msg_frequency   = 696;
            fsm_out.cw_dit_duration = 70;

            {
                int rand = random_bool() * 2 + random_bool();

                if (rand == 0) {
                    fsm_out.msg = "  HB9G";
                }
                else if (rand == 1) {
                    fsm_out.msg = "  HB9G JN36BK";
                }
                else if (rand == 2) {
                    fsm_out.msg = "  HB9G 1628M";
                }
                else {
                    fsm_out.msg = "  HB9G JN36BK 1628M";
                }
            }
            fsm_out.cw_psk31_trigger = 1;

            if (current_state == FSM_BALISE_COURTE) {

                if (fsm_in.sq) {
                    next_state = FSM_BALISE_COURTE_OPEN;
                } else {
                    if (fsm_in.cw_psk31_done) {
                        next_state = FSM_OISIF;
                    }
                }

            } else { //FSM_BALISE_COURTE_OPEN

                if (fsm_in.cw_psk31_done) {
                    if (fsm_in.sq) {
                        next_state = FSM_OPEN1;
                    } else {
                        next_state = FSM_OPEN2;
                    }
                }

            }

            break;
        default:
            // Should never happen
            next_state = FSM_OISIF;
            break;
    }


    if (next_state != current_state) {
        timestamp_state[next_state] = timestamp_now();

        short_beacon_counter_last_update = 0;

        switch (next_state) {
            case FSM_OISIF:
                fsm_state_switched("FSM_OISIF"); break;
            case FSM_OPEN1:
                fsm_state_switched("FSM_OPEN1"); break;
            case FSM_OPEN2:
                fsm_state_switched("FSM_OPEN2"); break;
            case FSM_LETTRE:
                fsm_state_switched("FSM_LETTRE"); break;
            case FSM_ECOUTE:
                fsm_state_switched("FSM_ECOUTE"); break;
            case FSM_ATTENTE:
                fsm_state_switched("FSM_ATTENTE"); break;
            case FSM_QSO:
                fsm_state_switched("FSM_QSO"); break;
            case FSM_ANTI_BAVARD:
                fsm_state_switched("FSM_ANTI_BAVARD"); break;
            case FSM_BLOQUE:
                fsm_state_switched("FSM_BLOQUE"); break;
            case FSM_TEXTE_73:
                fsm_state_switched("FSM_TEXTE_73"); break;
            case FSM_TEXTE_HB9G:
                fsm_state_switched("FSM_TEXTE_HB9G"); break;
            case FSM_TEXTE_LONG:
                fsm_state_switched("FSM_TEXTE_LONG"); break;
            case FSM_BALISE_LONGUE:
                fsm_state_switched("FSM_BALISE_LONGUE"); break;
            case FSM_BALISE_SPECIALE:
                fsm_state_switched("FSM_BALISE_SPECIALE"); break;
            case FSM_BALISE_COURTE:
                fsm_state_switched("FSM_BALISE_COURTE"); break;
            case FSM_BALISE_COURTE_OPEN:
                fsm_state_switched("FSM_BALISE_COURTE_OPEN"); break;
            default:
                fsm_state_switched("ERROR!"); break;
        }
    }
    current_state = next_state;
}

void fsm_update_inputs(struct fsm_input_signals_t* inputs)
{
    fsm_in = *inputs;
}

void fsm_get_outputs(struct fsm_output_signals_t* out)
{
    *out = fsm_out;
}
