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

#pragma once
#include <stdint.h>

// List of all states the FSM of the relay can be in
enum fsm_state_e {
    FSM_OISIF = 0,       // Idle
    FSM_OPEN1,           // 1750 Hz received and squelch open
    FSM_OPEN2,           // Squelch closed
    FSM_LETTRE,          // Transmit single status letter
    FSM_ECOUTE,          // Repeater open, waiting for QSO
    FSM_ATTENTE,         // No QSO after a short while
    FSM_QSO,             // QSO ongoing
    FSM_ANTI_BAVARD,     // QSO too long, cut transmission
    FSM_BLOQUE,          // Backoff after ANTI_BAVARD
    FSM_TEXTE_73,        // Transmit 73 after QSO
    FSM_TEXTE_HB9G,      // Transmit HB9G after QSO
    FSM_TEXTE_LONG,      // Transmit either HB9G JN36BK or HB9G 1628M after QSO
    FSM_BALISE_LONGUE,   // Full-length 2-hour beacon
    FSM_BALISE_SPECIALE, // 2-hour beacon when in QRP or with high power return mode
    FSM_BALISE_COURTE,   // Short intermittent beacon
    FSM_BALISE_COURTE_OPEN,   // Short intermittent beacon, need to switch to OPEN
    _NUM_FSM_STATES      // Dummy state to count the number of states
};

typedef enum fsm_state_e fsm_state_t;

// List of all states the balise FSM of the relay can be in
enum balise_fsm_state_e {
     BALISE_FSM_EVEN_HOUR = 0, // Even hours.
     BALISE_FSM_ODD_HOUR,      // Odd hours
     BALISE_FSM_PENDING,       // Waiting for transmission of balise
};

typedef enum balise_fsm_state_e balise_fsm_state_t;


// List of all states the SSTV FSM of the relay can be in
enum sstv_fsm_state_e {
    SSTV_FSM_OFF = 0,
    SSTV_FSM_ON,
};

typedef enum sstv_fsm_state_e sstv_fsm_state_t;


// All signals that the FSM can read, most of them are actually booleans
struct fsm_input_signals_t {
    /* Signals coming from repeater electronics */
    int sq;                // Squelch detection
    int discrim_u;         // FM discriminator says RX is too high in frequency
    int qrp;               // The relay is currently running with low power
    int hour_is_even;      // 1 if hour is even
    float temp;            // temperature in degrees C
    float humidity;        // relative humidity, range [0-100] %
    int wind_generator_ok; // false if the generator is folded out of the wind
    int discrim_d;         // FM discriminator says RX is too low in frequency
    int button_1750;       // Front panel button 1750Hz

    /* Signals coming from FAX and 1750 detector */
    int fax_mode;          // 1750Hz filter disabled for machine-generated modes
    int det_1750;          // 1750Hz detected

    /* Signals coming from CW and PSK generator */
    int cw_psk31_done;     // The CW and PSK generator has finished transmitting the message

    /* Signal coming from the standing wave ratio meter */
    int swr_high;          // We see a lot of return power

};

// All signals the FSM has to control
struct fsm_output_signals_t {
    /* Signals to the repeater electronics */
    int tx_on;             // Enable TX circuitry
    int modulation;        // Enable repeater RX to TX modulation

    /* Signals to the CW and PSK generator */
    const char* msg;       // The message to transmit
    int msg_frequency;     // What audio frequency for the CW or PSK message
    int cw_dit_duration;   // CW speed, dit duration in ms
    int cw_psk31_trigger;  // Set to true to trigger a CW or PSK31 transmission.
                           // PSK31 is sent if cw_dit_duration is 0

    /* Tone detector */
    int require_tone_detector; // Enables audio input and detector for DTMF and 1750
};

// Initialise local structures
void fsm_init(void);

// Call the FSM once and update the internal state
void fsm_update(void);

void fsm_balise_update(void);

// Update the FAX/SSTV state machine and return 1 if the 1750 notch filter must be
// disabled
int fsm_sstv_update(void);

// Force a BALISE
void fsm_balise_force(void);

// Setter for inputs
void fsm_update_inputs(struct fsm_input_signals_t* inputs);

// Getter for outputs
void fsm_get_outputs(struct fsm_output_signals_t* out);

// Announce a state change
void fsm_state_switched(const char *new_state);
