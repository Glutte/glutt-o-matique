/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Matthias P. Braendli, Maximilien Cuony
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
#include "Core/stats.h"
#include "GPIO/usart.h"
#include "GPIO/temperature.h"
#include "GPIO/batterycharge.h"
#include "GPIO/analog.h"

static struct fsm_input_signals_t fsm_in;
static struct fsm_output_signals_t fsm_out;

static fsm_state_t current_state;
static balise_fsm_state_t balise_state;
static sstv_fsm_state_t sstv_state;

// Keep track of when we last entered a given state, measured
// in ms using the timestamp_now() function
static uint64_t timestamp_state[_NUM_FSM_STATES];

static int last_battery_capacity_ah = 0;

#define BALISE_MESSAGE_LEN 75
static char balise_message[BALISE_MESSAGE_LEN];

static int balise_message_empty(void)
{
    return balise_message[0] == '\0';
}

static void balise_message_clear(void)
{
    balise_message[0] = '\0';
}


// Each 20 minutes, send a SHORT_BEACON
#define SHORT_BEACON_MAX (60 * 20)
// Reset the counter if the QSO was 10m too long
#define SHORT_BEACON_RESET_IF_QSO (60 * 10)

#define CALL "HB9G"

/* At least 1 second predelay for CW, ensures the receivers had enough time
 * time to open their squelch before the first letter gets transmitted
 */
#define CW_PREDELAY "      "

// Some time to ensure we don't cut off the last letter
#define CW_POSTDELAY "  "

// The counter (up to 20 minutes) for the short balise
static int short_beacon_counter_s = 0;
static uint64_t short_beacon_counter_last_update = 0;

// The last start of the last qso
static uint64_t last_qso_start_timestamp = 0;

// Information from which we can calculate the QSO duration
static struct {
    int qso_occurred;
    uint64_t qso_start_time;
} qso_info;

void fsm_init() {
    memset(&fsm_in, 0, sizeof(fsm_in));
    memset(&fsm_out, 0, sizeof(fsm_out));

    memset(timestamp_state, 0, _NUM_FSM_STATES * sizeof(*timestamp_state));
    timestamp_state[FSM_OISIF] = timestamp_now();

    current_state = FSM_OISIF;
    balise_state = BALISE_FSM_EVEN_HOUR;
    sstv_state = SSTV_FSM_OFF;
    balise_message[0] = '\0';

    qso_info.qso_occurred = 0;
    qso_info.qso_start_time = timestamp_now();
}

// Calculate the time spent in the current state
static uint64_t fsm_current_state_time_ms(void) {
    return timestamp_now() - timestamp_state[current_state];
}

static uint64_t fsm_current_state_time_s(void) {
    return fsm_current_state_time_ms() / 1000;
}

static const char* state_name(fsm_state_t state) {
    switch (state) {
        case FSM_OISIF: return "FSM_OISIF";
        case FSM_OPEN1: return "FSM_OPEN1";
        case FSM_OPEN2: return "FSM_OPEN2";
        case FSM_LETTRE: return "FSM_LETTRE";
        case FSM_ECOUTE: return "FSM_ECOUTE";
        case FSM_ATTENTE: return "FSM_ATTENTE";
        case FSM_QSO: return "FSM_QSO";
        case FSM_ANTI_BAVARD: return "FSM_ANTI_BAVARD";
        case FSM_BLOQUE: return "FSM_BLOQUE";
        case FSM_TEXTE_73: return "FSM_TEXTE_73";
        case FSM_TEXTE_HB9G: return "FSM_TEXTE_HB9G";
        case FSM_TEXTE_LONG: return "FSM_TEXTE_LONG";
        case FSM_BALISE_LONGUE: return "FSM_BALISE_LONGUE";
        case FSM_BALISE_STATS1: return "FSM_BALISE_STATS1";
        case FSM_BALISE_STATS2: return "FSM_BALISE_STATS2";
        case FSM_BALISE_STATS3: return "FSM_BALISE_STATS3";
        case FSM_BALISE_SPECIALE: return "FSM_BALISE_SPECIALE";
        case FSM_BALISE_SPECIALE_STATS1: return "FSM_BALISE_SPECIALE_STATS1";
        case FSM_BALISE_SPECIALE_STATS2: return "FSM_BALISE_SPECIALE_STATS2";
        case FSM_BALISE_SPECIALE_STATS3: return "FSM_BALISE_SPECIALE_STATS3";
        case FSM_BALISE_COURTE: return "FSM_BALISE_COURTE";
        case FSM_BALISE_COURTE_OPEN: return "FSM_BALISE_COURTE_OPEN";
        default: return "ERROR!";
    }
}

static const char* balise_state_name(balise_fsm_state_t state) {
    switch (state) {
        case BALISE_FSM_EVEN_HOUR: return "BALISE_FSM_EVEN_HOUR";
        case BALISE_FSM_ODD_HOUR: return "BALISE_FSM_ODD_HOUR";
        case BALISE_FSM_PENDING: return "BALISE_FSM_PENDING";
        default: return "ERROR!";
    }
}

static const char* sstv_state_name(sstv_fsm_state_t state) {
    switch (state) {
        case SSTV_FSM_OFF: return "SSTV_FSM_OFF";
        case SSTV_FSM_ON: return "SSTV_FSM_ON";
        default: return "ERROR!";
    }
}

static fsm_state_t select_grande_balise(void) {
    if (fsm_in.qrp || fsm_in.swr_high) {
        if (fsm_in.send_stats) {
            return FSM_BALISE_SPECIALE_STATS1;
        }
        else {
            return FSM_BALISE_SPECIALE;
        }
    }
    else if (fsm_in.send_stats) {
        return FSM_BALISE_STATS1;
    }
    else {
        return FSM_BALISE_LONGUE;
    }
}

static uint64_t qso_duration(void) {
    return timestamp_state[current_state] - qso_info.qso_start_time;
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

static const char* fsm_select_letter(void) {
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
    else if (sstv_state == SSTV_FSM_ON) {
        return letter_sstv;
    }

    return letter_all_ok;
}


void fsm_update() {

    fsm_state_t next_state = current_state;

    // Some defaults for the outgoing signals
    fsm_out.tx_on = 0;
    fsm_out.modulation = 0;
    fsm_out.cw_psk_trigger = 0;
    fsm_out.cw_dit_duration = 50;
    fsm_out.msg_frequency = 960;
    fsm_out.require_tone_detector = 0;
    // other output signals keep their value

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
            while(short_beacon_counter_s < SHORT_BEACON_MAX && (fsm_current_state_time_s() - short_beacon_counter_last_update > 1)) {
                short_beacon_counter_last_update++;
                short_beacon_counter_s++;
            }

            // SQ and button 1750 are debounced inside pio.c (300ms)
            fsm_out.require_tone_detector = fsm_in.sq;

            if ( (fsm_in.sq && fsm_in.det_1750) ||
                 (fsm_in.sq && sstv_state == SSTV_FSM_ON) ||
                 (fsm_in.button_1750)) {
                next_state = FSM_OPEN1;
            }
            else if (balise_state == BALISE_FSM_PENDING) {
                short_beacon_counter_s = 0;
                next_state = select_grande_balise();
            }
            else if (!fsm_in.qrp && short_beacon_counter_s == SHORT_BEACON_MAX) {
                short_beacon_counter_s = 0;
                next_state = FSM_BALISE_COURTE;
            }

            break;

        case FSM_OPEN1:
            /* Do not enable TX_ON here, otherwise we could get stuck transmitting
             * forever if SQ never goes low.
             */
            fsm_out.require_tone_detector = 1;
            if (!fsm_in.sq && !fsm_in.det_1750) {
                next_state = FSM_OPEN2;
            }
            break;

        case FSM_OPEN2:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;
            fsm_out.require_tone_detector = 1;
            qso_info.qso_occurred = 0;
            qso_info.qso_start_time = timestamp_now();

            if (fsm_current_state_time_ms() > 200) {
                next_state = FSM_LETTRE;
            }
            break;

        case FSM_LETTRE:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;
            fsm_out.require_tone_detector = 1;
            fsm_out.msg = fsm_select_letter();
            if (fsm_out.msg[0] == 'G') {
                // The letter 'G' is a bit different
                fsm_out.msg_frequency    = 696;
            }
            fsm_out.cw_psk_trigger = 1;

            if (fsm_in.cw_psk_done) {
                next_state = FSM_ECOUTE;
            }
            break;

        case FSM_ECOUTE:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;
            fsm_out.require_tone_detector = 1;

            /* Time checks:
             * We need to check the total TX_ON duration to decide the text to
             * send. This is the QSO duration.
             *
             * We also need to check if we actually entered the QSO state
             * recently, otherwise we want to go to ATTENTE. That's why the
             * additional field qso_occurred is required.
             */

            if (fsm_in.sq) {
                next_state = FSM_QSO;
            }
            else {
                if (fsm_current_state_time_s() > 5) {
                    if (balise_state == BALISE_FSM_PENDING) {
                        short_beacon_counter_s = 0;
                        next_state = select_grande_balise();
                    }
                    else if (qso_info.qso_occurred) {
                        if (qso_duration() >= 1000ul * 15 * 60) {
                            next_state = FSM_TEXTE_LONG;
                        }
                        else if (qso_duration() >= 1000ul * 10 * 60) {
                            next_state = FSM_TEXTE_HB9G;
                        }
                        else if (qso_duration() >= 1000ul * 5 * 60) {
                            next_state = FSM_TEXTE_73;
                        }
                        else {
                            next_state = FSM_OISIF;
                        }
                    }
                }

                if (fsm_current_state_time_s() > 6 && !qso_info.qso_occurred) {
                    next_state = FSM_ATTENTE;
                }

                /* If everything fails and the state was not changed after 7
                 * seconds, fall back to oisif
                 */
                if (fsm_current_state_time_s() > 7) {
                    next_state = FSM_OISIF;
                }
            }
            break;

        case FSM_ATTENTE:
            if (fsm_in.sq) {
                fsm_out.require_tone_detector = 1;
                next_state = FSM_ECOUTE;
            }
            else if (fsm_current_state_time_s() > 15) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_QSO:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;
            fsm_out.require_tone_detector = 1;
            qso_info.qso_occurred = 1;

            // Save the starting timestamp, if there is none
            if (last_qso_start_timestamp == 0) {
                last_qso_start_timestamp = timestamp_now();
            }

            if (!fsm_in.sq && fsm_current_state_time_s() < 3) {
                /* To avoid that very short open squelch triggers
                 * transmit CW letters all the time. Some people
                 * enjoy doing that.
                 */
                next_state = FSM_ECOUTE;
            }
            else if (!fsm_in.sq && fsm_current_state_time_s() >= 3) {
                next_state = FSM_LETTRE;
            }
            else if (fsm_current_state_time_s() > 5 * 60) {
                next_state = FSM_ANTI_BAVARD;
            }
            break;

        case FSM_ANTI_BAVARD:
            fsm_out.tx_on = 1;
            // No modulation!

            // Short post-delay to underscore the fact that
            // transmission was forcefully cut off.
            fsm_out.msg = " HI HI ";
            fsm_out.cw_psk_trigger = 1;

            if (fsm_in.cw_psk_done) {
                stats_anti_bavard_triggered();
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
            fsm_out.require_tone_detector = 1;
            fsm_out.msg_frequency    = 696;
            fsm_out.cw_dit_duration = 70;
            fsm_out.msg = " 73" CW_POSTDELAY;
            fsm_out.cw_psk_trigger = 1;

            if (fsm_in.sq) {
                next_state = FSM_QSO;
                qso_info.qso_start_time = timestamp_now();
            }
            else if (fsm_in.cw_psk_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_TEXTE_HB9G:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;
            fsm_out.require_tone_detector = 1;
            fsm_out.msg_frequency   = 696;
            fsm_out.cw_dit_duration = 70;
            // No need for CW_PREDELAY, since we are already transmitting
            fsm_out.msg = " " CALL CW_POSTDELAY;
            fsm_out.cw_psk_trigger = 1;

            if (fsm_in.sq) {
                next_state = FSM_QSO;
                qso_info.qso_start_time = timestamp_now();
            }
            else if (fsm_in.cw_psk_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_TEXTE_LONG:
            fsm_out.tx_on = 1;
            fsm_out.modulation = 1;
            fsm_out.require_tone_detector = 1;

            fsm_out.msg_frequency   = 696;
            fsm_out.cw_dit_duration = 70;

            // No need for CW_PREDELAY, since we are already transmitting
            if (random_bool()) {
                fsm_out.msg = " " CALL " 1628M" CW_POSTDELAY;
            }
            else {
                fsm_out.msg = " " CALL " JN36BK" CW_POSTDELAY;
            }
            fsm_out.cw_psk_trigger = 1;

            if (fsm_in.sq) {
                next_state = FSM_QSO;
                qso_info.qso_start_time = timestamp_now();
            }
            else if (fsm_in.cw_psk_done) {
                next_state = FSM_OISIF;
            }
            break;

        case FSM_BALISE_LONGUE:
        case FSM_BALISE_STATS1:
            fsm_out.tx_on = 1;
            fsm_out.msg_frequency   = 588;
            fsm_out.cw_dit_duration = 110;

            {
                const float supply_voltage = round_float_to_half_steps(analog_measure_12v());
                const int supply_decivolts = supply_voltage * 10.0f;

                const char *eol_info = "73";
                if (current_state == FSM_BALISE_STATS1) {
                    eol_info = "PSK125";
                }
                else if (batterycharge_wind_disconnected() == 1) {
                    eol_info = "EOL \\"; // backslash is <SK>
                }
                else if (!fsm_in.wind_generator_ok) {
                    eol_info = "\\"; // backslash is <SK>
                }

                if (balise_message_empty()) {
                    const uint32_t capacity_bat_mah = batterycharge_retrieve_last_capacity();
                    const int capacity_bat_ah = capacity_bat_mah / 1000;

                    char supply_trend = '=';
                    if (capacity_bat_ah) {
                        // = means same battery capacity as previous
                        // + means higher
                        // - means lower
                        if (last_battery_capacity_ah < capacity_bat_ah) {
                            supply_trend = '+';
                        }
                        else if (last_battery_capacity_ah > capacity_bat_ah) {
                            supply_trend = '-';
                        }

                        last_battery_capacity_ah = capacity_bat_ah;
                    }

                    size_t len = 0;

                    len += snprintf(balise_message + len, BALISE_MESSAGE_LEN-len-1,
                            CW_PREDELAY CALL " JN36BK  U %dV%01d ",
                                supply_decivolts / 10,
                                supply_decivolts % 10);

                    if (capacity_bat_ah != 0) {
                        len += snprintf(balise_message + len, BALISE_MESSAGE_LEN-len-1,
                                " %d AH %c ", capacity_bat_ah, supply_trend);
                    }

                    float temp = 0;
                    if (temperature_get(&temp)) {
                        len += snprintf(balise_message + len, BALISE_MESSAGE_LEN-len-1,
                                " T %d ",
                                (int)(round_float_to_half_steps(temp)));
                    }

                    snprintf(balise_message + len, BALISE_MESSAGE_LEN-len-1,
                            "%s" CW_POSTDELAY,
                            eol_info);

                    fsm_out.msg = balise_message;
                    fsm_out.cw_psk_trigger = 1;
                }
            }

            if (fsm_in.cw_psk_done) {
                balise_message_clear();
                // The exercise_fsm loop needs to see a 1 to 0 transition on cw_psk_trigger
                // so that it considers the STATS2 message.
                fsm_out.cw_psk_trigger = 0;
                if (current_state == FSM_BALISE_STATS1) {
                    fsm_out.msg = NULL;
                    next_state = FSM_BALISE_STATS2;
                }
                else {
                    next_state = FSM_OISIF;
                }
            }
            break;

        case FSM_BALISE_STATS2:
        case FSM_BALISE_SPECIALE_STATS2:
            fsm_out.tx_on = 1;
            fsm_out.msg_frequency   = 588;
            fsm_out.cw_dit_duration = -3; // PSK125

            // All predecessor states must NULL the fsm_out.msg field!
            if (fsm_out.msg == NULL) {
                fsm_out.msg = stats_build_text(batterycharge_wind_disconnected() == 1);
            }
            fsm_out.cw_psk_trigger = 1;

            if (fsm_in.cw_psk_done) {
                fsm_out.cw_psk_trigger = 0;
                next_state = (current_state == FSM_BALISE_STATS2) ?
                    FSM_BALISE_STATS3 :
                    FSM_BALISE_SPECIALE_STATS3;
            }
            break;

        case FSM_BALISE_STATS3:
        case FSM_BALISE_SPECIALE_STATS3:
            fsm_out.tx_on = 1;
            if (current_state == FSM_BALISE_STATS3) {
                fsm_out.msg_frequency   = 588;
                fsm_out.cw_dit_duration = 110;
            }
            else {
                fsm_out.msg_frequency   = 696;
                fsm_out.cw_dit_duration = 70;
            }

            if (balise_message_empty()) {
                const char *eol_info = "73";
                if (batterycharge_wind_disconnected() == 1) {
                    eol_info = "EOL \\"; // backslash is <SK>
                }
                else if (!fsm_in.wind_generator_ok) {
                    eol_info = "\\"; // backslash is <SK>
                }
                snprintf(balise_message, BALISE_MESSAGE_LEN-1,
                        CW_PREDELAY "%s" CW_POSTDELAY,
                        eol_info);
                fsm_out.msg = balise_message;
                fsm_out.cw_psk_trigger = 1;
            }

            if (fsm_in.cw_psk_done) {
                stats_beacon_sent();
                fsm_out.cw_psk_trigger = 1;
                balise_message_clear();
                next_state = FSM_OISIF;
            }
            break;

        case FSM_BALISE_SPECIALE:
        case FSM_BALISE_SPECIALE_STATS1:
            fsm_out.tx_on = 1;
            fsm_out.msg_frequency   = 696;
            fsm_out.cw_dit_duration = 70;

            if (balise_message_empty()) {
                const float supply_voltage = round_float_to_half_steps(analog_measure_12v());
                const int supply_decivolts = supply_voltage * 10.0f;

                const char *eol_info = "73";
                if (current_state == FSM_BALISE_SPECIALE_STATS1) {
                    eol_info = "PSK125";
                }
                else if (batterycharge_wind_disconnected() == 1) {
                    eol_info = "EOL \\"; // backslash is <SK>
                }
                else if (!fsm_in.wind_generator_ok) {
                    eol_info = "\\"; // backslash is <SK>
                }

                size_t len = 0;
                len += snprintf(balise_message+len, BALISE_MESSAGE_LEN-len-1,
                        CW_PREDELAY CALL " U %dV%01d ",
                        supply_decivolts / 10,
                        supply_decivolts % 10);

                const uint32_t capacity_bat_mah = batterycharge_retrieve_last_capacity();
                const int capacity_bat_ah = capacity_bat_mah / 1000;
                if (capacity_bat_ah != 0) {
                    len += snprintf(balise_message + len, BALISE_MESSAGE_LEN-len-1,
                            " %d AH ", capacity_bat_ah);
                }

                float temp = 0;
                if (temperature_get(&temp)) {
                    len += snprintf(balise_message + len, BALISE_MESSAGE_LEN-len-1,
                            " T %d ",
                            (int)(round_float_to_half_steps(temp)));
                }

                len += snprintf(balise_message+len, BALISE_MESSAGE_LEN-len-1,
                        "%s" CW_POSTDELAY,
                        eol_info);

                fsm_out.msg = balise_message;

                fsm_out.cw_psk_trigger = 1;
            }

            if (fsm_in.cw_psk_done) {
                stats_beacon_sent();
                balise_message_clear();
                // The exercise_fsm loop needs to see a 1 to 0 transition on cw_psk_trigger
                // so that it considers the STATS2 message.
                fsm_out.cw_psk_trigger = 0;
                if (current_state == FSM_BALISE_SPECIALE_STATS1) {
                    fsm_out.msg = NULL;
                    next_state = FSM_BALISE_SPECIALE_STATS2;
                }
                else {
                    next_state = FSM_OISIF;
                }
            }
            break;

        case FSM_BALISE_COURTE:
        case FSM_BALISE_COURTE_OPEN:

            fsm_out.tx_on = 1;

            fsm_out.msg_frequency   = 696;
            fsm_out.cw_dit_duration = 70;

            if (fsm_in.bonne_annee) {
                fsm_out.msg = CW_PREDELAY CALL "  BONNE ANNEE" CW_POSTDELAY;
            }
            else {
                int rand = random_bool() * 2 + random_bool();

                if (rand == 0) {
                    fsm_out.msg = CW_PREDELAY CALL CW_POSTDELAY;
                }
                else if (rand == 1) {
                    fsm_out.msg = CW_PREDELAY CALL " JN36BK" CW_POSTDELAY;
                }
                else if (rand == 2) {
                    fsm_out.msg = CW_PREDELAY CALL " 1628M" CW_POSTDELAY;
                }
                else {
                    fsm_out.msg = CW_PREDELAY CALL " JN36BK  1628M" CW_POSTDELAY;
                }
            }
            fsm_out.cw_psk_trigger = 1;

            if (current_state == FSM_BALISE_COURTE) {
                if (fsm_in.cw_psk_done) {
                    if (fsm_in.sq) {
                        next_state = FSM_OPEN2;
                    }
                    else {
                        stats_beacon_sent();
                        next_state = FSM_OISIF;
                    }
                }
                else if (fsm_in.sq) {
                    next_state = FSM_BALISE_COURTE_OPEN;
                }
            }
            else { //FSM_BALISE_COURTE_OPEN
                if (fsm_in.cw_psk_done) {
                    stats_beacon_sent();
                    next_state = FSM_OPEN2;
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

        fsm_state_switched(state_name(next_state));
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

void fsm_balise_force() {
    balise_state = BALISE_FSM_PENDING;
}

void fsm_balise_update() {

    balise_fsm_state_t next_state = balise_state;

    switch (balise_state) {
        case BALISE_FSM_EVEN_HOUR:
            if (fsm_in.hour_is_even == 0) {
                next_state = BALISE_FSM_ODD_HOUR;
            }
            break;
        case BALISE_FSM_ODD_HOUR:
            if (fsm_in.hour_is_even == 1) {
                if (timestamp_now() > 1000 * 60) { // Does not start the balise at startup
                    next_state = BALISE_FSM_PENDING;
                }
                else {
                    next_state = BALISE_FSM_EVEN_HOUR;
                }
            }
            break;
        case BALISE_FSM_PENDING:
            if (current_state == FSM_BALISE_SPECIALE ||
                    current_state == FSM_BALISE_LONGUE ||
                    current_state == FSM_BALISE_STATS3 ||
                    current_state == FSM_BALISE_SPECIALE_STATS3) {
                next_state = BALISE_FSM_EVEN_HOUR;
            }
            break;
        default:
            // Should never happen
            next_state = BALISE_FSM_EVEN_HOUR;
            break;
    }

    if (next_state != balise_state) {
        fsm_state_switched(balise_state_name(next_state));
    }

    balise_state = next_state;
}

int fsm_sstv_update() {

    sstv_fsm_state_t next_state = sstv_state;

    switch (sstv_state) {
        case SSTV_FSM_OFF:
            if (fsm_in.sq && fsm_in.fax_mode) {
                next_state = SSTV_FSM_ON;
            }
            break;
        case SSTV_FSM_ON:
            if (current_state == FSM_BALISE_LONGUE ||
                current_state == FSM_ANTI_BAVARD ||
                current_state == FSM_BALISE_SPECIALE ||
                fsm_in.long_1750
            ) {
                next_state = SSTV_FSM_OFF;
            }
            break;

        default:
            // Should never happen
            next_state = SSTV_FSM_OFF;
            break;
    }

    if (next_state != sstv_state) {
        fsm_state_switched(sstv_state_name(next_state));
    }

    sstv_state = next_state;

    return sstv_state == SSTV_FSM_ON;
}
