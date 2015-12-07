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

#include "cw.h"
#include "arm_math.h"
#include "audio.h"
#include "debug.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#define ON_BUFFER_SIZE 1024

struct cw_out_message_s {
    // Contains a sequence of ones and zeros corresponding to
    // TX on/TX off CW data to be sent
    uint8_t on_buffer[ON_BUFFER_SIZE];
    size_t  on_buffer_end;

    int     freq;
    int     dit_duration;
};

// The queue contains above structs
QueueHandle_t cw_queue;
static int    cw_samplerate;

// The message being currently handled by cw_fill_buffer
static struct cw_out_message_s cw_fill_msg_current;
// Set to 1 if the cw_fill_buffer function is currently handling
// msg_current. 0 if it has to wait for a new message.
static int    cw_fill_msg_status;

// Keep track of number of audio samples that were already
// generated from the current message
static int    cw_fill_audio_sent;


void cw_init(unsigned int samplerate)
{
    cw_samplerate = samplerate;

    cw_fill_msg_status = 0;

    cw_queue = xQueueCreate(15, sizeof(struct cw_out_message_s));
    if (cw_queue == 0) {
        while(1); /* fatal error */
    }
}

const uint8_t cw_mapping[60] = { // {{{
    // Read bits from right to left

    0b110101, //+ ASCII 43
    0b110101, //, ASCII 44
    0b1011110, //- ASCII 45

    0b1010101, //., ASCII 46
    0b110110, // / ASCII 47

    0b100000, // 0, ASCII 48
    0b100001, // 1
    0b100011,
    0b100111,
    0b101111,
    0b111111,
    0b111110,
    0b111100,
    0b111000,
    0b110000, // 9, ASCII 57

    // The following are mostly invalid, but
    // required to fill the gap in ASCII between
    // numerals and capital letters
    0b10, // :
    0b10, // ;
    0b10, // <
    0b10, // =
    0b10, // >
    0b1110011, // ?
    0b1101001, //@

    0b101, // A  ASCII 65
    0b11110,
    0b11010,
    0b1110,
    0b11,
    0b11011,
    0b1100,
    0b11111,
    0b111,
    0b10001,
    0b1010,
    0b11101,
    0b100, //M
    0b110,
    0b1000,
    0b11001,
    0b10100,
    0b1101,
    0b1111,
    0b10,
    0b1011,
    0b10111,
    0b1001,
    0b10110,
    0b10010,
    0b11100, // Z

    0b101010, //Start, ASCII [
    0b1010111, // SK , ASCII '\'
}; //}}}

void cw_symbol(uint8_t sym, struct cw_out_message_s *msg)
{
    uint8_t p = 0;
    uint8_t val = cw_mapping[sym];

    while((val >> p) != 0b1) {

        if (((val >> p) & 0b1) == 0b1) {
            if (msg->on_buffer_end + 2 < ON_BUFFER_SIZE) {
                // tone(1)
                msg->on_buffer[msg->on_buffer_end++] = 1;

                // silence(1)
                msg->on_buffer[msg->on_buffer_end++] = 0;
            }
        }
        else {
            if (msg->on_buffer_end + 4 < ON_BUFFER_SIZE) {
                // tone(3)
                msg->on_buffer[msg->on_buffer_end++] = 1;
                msg->on_buffer[msg->on_buffer_end++] = 1;
                msg->on_buffer[msg->on_buffer_end++] = 1;

                // silence(1)
                msg->on_buffer[msg->on_buffer_end++] = 0;
            }
        }

        p++;
    }

    // silence(4)
    if (msg->on_buffer_end + 4 < ON_BUFFER_SIZE) {
        for (int i = 0; i < 4; i++) {
            msg->on_buffer[msg->on_buffer_end++] = 0;
        }
    }
}

// Transmit a string in morse code. Supported range:
// All ASCII between '+' and '\', which includes
// numerals and capital letters.
void cw_push_message(const char* text, int dit_duration, int frequency)
{
    struct cw_out_message_s msg;
    for (int i = 0; i < ON_BUFFER_SIZE; i++) {
        msg.on_buffer[i] = 0;
    }
    msg.on_buffer_end = 0;
    msg.freq = frequency;
    msg.dit_duration = dit_duration;

    const char* sym = text;
    do {
        if (*sym < '+' || *sym > '\\') {
            if (msg.on_buffer_end + 4 < ON_BUFFER_SIZE) {
                for (int i = 0; i < 4; i++) {
                    msg.on_buffer[msg.on_buffer_end++] = 0;
                }
            }
        }
        else {
            cw_symbol(*sym - '+', &msg);
        }
        sym++;
    } while (*sym != '\0');

    xQueueSendToBack(cw_queue, &msg, 0); /* Send Message */
}

size_t cw_fill_buffer(int16_t *buf, size_t bufsize)
{
    static float nco_phase = 0.0f;


    if (cw_fill_msg_status == 0) {
        int waiting = uxQueueMessagesWaitingFromISR(cw_queue);
#if 1
        if (waiting > 0 &&
            xQueueReceiveFromISR(cw_queue, &cw_fill_msg_current, NULL)) {
            // Convert msg to audio samples and transmit
            cw_fill_msg_status = 1;
            cw_fill_audio_sent = 0;
        }
        else {
            return 0;
        }
#else
        return 0;
#endif
    }

#if 1
    const int samples_per_dit = (cw_samplerate * 10) /
        cw_fill_msg_current.dit_duration;

    // Angular frequency of NCO
    const float omega = 2.0f * FLOAT_PI * cw_fill_msg_current.freq /
        (float)cw_samplerate;

    // Define start point
    int start_i = cw_fill_audio_sent / samples_per_dit;
    int start_t = cw_fill_audio_sent % samples_per_dit;

    int pos = 0;

    for (int i = start_i; i < cw_fill_msg_current.on_buffer_end; i++) {

        for (int t = start_t; t < samples_per_dit; t++) {
            int16_t s = 0;

            if (cw_fill_msg_current.on_buffer[i]) {
                nco_phase += omega;
                if (nco_phase > FLOAT_PI) {
                    nco_phase -= 2.0f * FLOAT_PI;
                }

                s = 32768.0f * arm_sin_f32(nco_phase);
            }

            if (pos + 2 >= bufsize) {
                goto cw_fill_buf_full;
            }

            // Stereo
            buf[pos++] = s;
            buf[pos++] = s;

            cw_fill_audio_sent += 2;
        }

        start_t = 0;
    }

    // We have completed this message
    cw_fill_msg_status = 0;

cw_fill_buf_full:
#else
    const float omega = 2.0f * FLOAT_PI * 300.0f /
                        (float)cw_samplerate;

    for (int t = 0; t < bufsize; t++) {
        int16_t s = 0;

        nco_phase += omega;
        if (nco_phase > FLOAT_PI) {
            nco_phase -= 2.0f * FLOAT_PI;
        }

        // TODO preserve oscillator phase
        s = 32768.0f * arm_sin_f32(nco_phase);

        buf[t] = s;
    }
#endif
    return bufsize;
}

