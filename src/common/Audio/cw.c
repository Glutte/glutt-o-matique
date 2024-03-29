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

/* CW and BPSK{31,63,125} generator
 *
 * Concept:
 *
 * +-------------------+                    +----------------+
 * | cw_push_message() | -> cw_msg_queue -> | cw_psktask() | -> cw_audio_queue
 * +-------------------+                    +----------------+
 *
 * The cw_psk_fill_buffer() function can be called to fetch audio from the
 * audio_queue
 */

#include "Audio/cw.h"
#include "Core/common.h"
#include "Audio/audio.h"
#include <string.h>

#ifdef SIMULATOR
#include <math.h>
#define arm_cos_f32 cosf
#define arm_sin_f32 sinf
#else
#include "arm_math.h"
#endif

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define MAX_MESSAGE_LEN 1024
#define MAX_ON_BUFFER_LEN 8192

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
    0b101110, // =
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

/*
 * PSK Varicode
 * http://aintel.bi.ehu.es/psk31.html
 */
const char *psk_varicode[] = { // {{{
    "1010101011",
    "1011011011",
    "1011101101",
    "1101110111",
    "1011101011",
    "1101011111",
    "1011101111",
    "1011111101",
    "1011111111",
    "11101111",
    "11101",
    "1101101111",
    "1011011101",
    "11111",
    "1101110101",
    "1110101011",
    "1011110111",
    "1011110101",
    "1110101101",
    "1110101111",
    "1101011011",
    "1101101011",
    "1101101101",
    "1101010111",
    "1101111011",
    "1101111101",
    "1110110111",
    "1101010101",
    "1101011101",
    "1110111011",
    "1011111011",
    "1101111111",
    "1",
    "111111111",
    "101011111",
    "111110101",
    "111011011",
    "1011010101",
    "1010111011",
    "101111111",
    "11111011",
    "11110111",
    "101101111",
    "111011111",
    "1110101",
    "110101",
    "1010111",
    "110101111",
    "10110111",
    "10111101",
    "11101101",
    "11111111",
    "101110111",
    "101011011",
    "101101011",
    "110101101",
    "110101011",
    "110110111",
    "11110101",
    "110111101",
    "111101101",
    "1010101",
    "111010111",
    "1010101111",
    "1010111101",
    "1111101",
    "11101011",
    "10101101",
    "10110101",
    "1110111",
    "11011011",
    "11111101",
    "101010101",
    "1111111",
    "111111101",
    "101111101",
    "11010111",
    "10111011",
    "11011101",
    "10101011",
    "11010101",
    "111011101",
    "10101111",
    "1101111",
    "1101101",
    "101010111",
    "110110101",
    "101011101",
    "101110101",
    "101111011",
    "1010101101",
    "111110111",
    "111101111",
    "111111011",
    "1010111111",
    "101101101",
    "1011011111",
    "1011",
    "1011111",
    "101111",
    "101101",
    "11",
    "111101",
    "1011011",
    "101011",
    "1101",
    "111101011",
    "10111111",
    "11011",
    "111011",
    "1111",
    "111",
    "111111",
    "110111111",
    "10101",
    "10111",
    "101",
    "110111",
    "1111011",
    "1101011",
    "11011111",
    "1011101",
    "111010101",
    "1010110111",
    "110111011",
    "1010110101",
    "1011010111",
    "1110110101",
}; //}}}

// Function to display message in GUI
void cw_message_sent(const char* str);

struct cw_message_s {
    char          message[MAX_MESSAGE_LEN];
    size_t        message_len;

    int           freq;

    // If dit_duration is negative, the message is sent in PSK
    int           dit_duration;
};

// The queue contains above structs
QueueHandle_t cw_msg_queue;

// Queue that contains audio data
QueueHandle_t cw_audio_queue;
static int    cw_psk_samplerate;

static int    cw_transmit_ongoing;

static void cw_psk_task(void *pvParameters);

void cw_psk_init(unsigned int samplerate)
{
    cw_psk_samplerate = samplerate;
    cw_transmit_ongoing = 0;

    cw_msg_queue = xQueueCreate(10, sizeof(struct cw_message_s));
    if (cw_msg_queue == 0) {
        while(1); /* fatal error */
    }

    cw_audio_queue = xQueueCreate(1, AUDIO_BUF_LEN * sizeof(int16_t));
    if (cw_audio_queue == 0) {
        while(1); /* fatal error */
    }

    xTaskCreate(
            cw_psk_task,
            "CWPSKTask",
            8*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 3UL,
            NULL);
}

/* Parse one CW letter/symbol, and fill in the on_buffer.
 * Returns number of uint8_t written.
 */
size_t cw_symbol(uint8_t sym, uint8_t *on_buffer, size_t on_buffer_size)
{
    uint8_t p = 0;
    uint8_t val = cw_mapping[sym];
    size_t pos = 0;

    while((val >> p) != 0b1) {

        if (((val >> p) & 0b1) == 0b1) {
            if (pos + 2 < on_buffer_size) {
                // tone(1)
                on_buffer[pos++] = 1;

                // silence(1)
                on_buffer[pos++] = 0;
            }
        }
        else {
            if (pos + 4 < on_buffer_size) {
                // tone(3)
                on_buffer[pos++] = 1;
                on_buffer[pos++] = 1;
                on_buffer[pos++] = 1;

                // silence(1)
                on_buffer[pos++] = 0;
            }
        }

        p++;
    }

    // silence(2)
    if (pos + 2 < on_buffer_size) {
        for (int i = 0; i < 2; i++) {
            on_buffer[pos++] = 0;
        }
    }

    return pos;
}

int cw_psk_push_message(const char* text, int dit_duration, int frequency)
{
    const int text_len = strlen(text);

    if (strlen(text) > MAX_MESSAGE_LEN) {
        return 0;
    }

    struct cw_message_s msg;
    for (int i = 0; i < MAX_MESSAGE_LEN; i++) {
        if (i < text_len) {
            msg.message[i] = text[i];
        }
        else {
            msg.message[i] = '\0';
        }
    }
    msg.message_len = text_len;
    msg.freq = frequency;
    msg.dit_duration = dit_duration;

    if (xQueueSendToBack(cw_msg_queue, &msg, portMAX_DELAY) != pdTRUE) {
        trigger_fault(FAULT_SOURCE_CW_QUEUE);
    }

    cw_message_sent(msg.message);

    return 1;
}

/* Parse the message and fill the on_buffer with CW on/CW off information.
 * Returns the number of on/off bits written.
 */
static size_t cw_text_to_on_buffer(const char *msg, uint8_t *on_buffer, size_t on_buffer_size)
{
    size_t pos = 0;
    const char* sym = msg;
    do {
        if (*sym < '+' || *sym > '\\') {
            if (pos + 3 < on_buffer_size) {
                for (int i = 0; i < 3; i++) {
                    on_buffer[pos++] = 0;
                }
            }
        }
        else {
            pos += cw_symbol(*sym - '+', on_buffer + pos, on_buffer_size - pos);
        }
        sym++;
    } while (*sym != '\0');

    return pos;
}


/*
 * Turn a null terminated ASCII string into a uint8_t buffer
 * of 0 and 1 representing the PSK varicode for the input.
 *
 * outstr must be at least size 20 + strlen(instr)*12 + 20 to accomodate
 * the header and tail.
 *
 * Returns number of bytes written.
 */
static size_t psk_text_to_phase_buffer(const char* instr, uint8_t* outbits)
{
    int i=0, j, k;

    /* Header of 0s */
    for (j=0; j < 20; j++) {
        outbits[i++] = 0;
    }

    /* Encode the message, with 00 between letters */
    for (j=0; j < strlen(instr); j++) {
        const uint16_t ix = instr[j];
        if (ix < sizeof(psk_varicode)) {
            const char* varicode_bits = psk_varicode[ix];
            for(k=0; k < strlen(varicode_bits); k++) {
                outbits[i++] = (varicode_bits[k] == '1') ? 1 : 0;
            }
            outbits[i++] = 0;
            outbits[i++] = 0;
        }
    }

    /* Tail of 0s */
    for (j=0; j < 20; j++) {
        outbits[i++] = 0;
    }

    return i;
}


size_t cw_psk_fill_buffer(int16_t *buf, size_t bufsize)
{
    if (xQueueReceiveFromISR(cw_audio_queue, buf, NULL)) {
        return bufsize;
    }
    else {
        return 0;
    }
}

static int16_t cw_audio_buf[AUDIO_BUF_LEN];
static uint8_t cw_psk_buffer[MAX_ON_BUFFER_LEN];
static struct cw_message_s cw_fill_msg_current;

// Routine to generate CW audio
static float cw_generate_audio_ampl = 0.0f;
static float cw_generate_audio_nco = 0.0f;
static int16_t cw_generate_audio(float omega, int i, int __attribute__ ((unused))t)
{
    int16_t s = 0;
    // Remove clicks from CW
    if (cw_psk_buffer[i]) {
        const float remaining = 32768.0f - cw_generate_audio_ampl;
        cw_generate_audio_ampl += remaining / 64.0f;
    }
    else {
        cw_generate_audio_ampl -= cw_generate_audio_ampl / 64.0f;

        if (cw_generate_audio_ampl < 0) {
            cw_generate_audio_ampl = 0;
        }
    }

    cw_generate_audio_nco += omega;
    if (cw_generate_audio_nco > FLOAT_PI) {
        cw_generate_audio_nco -= 2.0f * FLOAT_PI;
    }

    s = cw_generate_audio_ampl * arm_sin_f32(cw_generate_audio_nco);
    return s;
}

static float psk_generate_audio_nco = 0.0f;
static int   psk_current_psk_phase = 1;
static int16_t psk_generate_audio(float omega, int i, int t, int samples_per_symbol)
{
    int16_t s = 0;
    const float base_ampl = 10000.0f;
    float psk_generate_audio_ampl = 0.0f;

    if (cw_psk_buffer[i] == 1) {
        psk_generate_audio_ampl = base_ampl;
    }
    else {
        psk_generate_audio_ampl = base_ampl * arm_cos_f32(
                FLOAT_PI*(float)t/(float)samples_per_symbol);
    }

    psk_generate_audio_nco += omega;
    if (psk_generate_audio_nco > FLOAT_PI) {
        psk_generate_audio_nco -= 2.0f * FLOAT_PI;
    }

    s = psk_generate_audio_ampl *
        arm_sin_f32(psk_generate_audio_nco +
            (psk_current_psk_phase == 1 ? 0.0f : FLOAT_PI));

    return s;
}

static void cw_psk_task(void __attribute__ ((unused))*pvParameters)
{
    int buf_pos = 0;

    while (1) {
        int status = xQueueReceive(cw_msg_queue, &cw_fill_msg_current, portMAX_DELAY);
        if (status == pdTRUE) {
            size_t cw_psk_buffer_len = 0;

            cw_transmit_ongoing = 1;

            if (cw_fill_msg_current.dit_duration == 0) {
                // Illegal
                cw_transmit_ongoing = 0;
                continue;
            }
            else if (cw_fill_msg_current.dit_duration > 0) {
                cw_psk_buffer_len = cw_text_to_on_buffer(
                        cw_fill_msg_current.message,
                        cw_psk_buffer,
                        MAX_ON_BUFFER_LEN);

            }
            else {
                cw_psk_buffer_len = psk_text_to_phase_buffer(
                        cw_fill_msg_current.message,
                        cw_psk_buffer);
            }

            // Angular frequency of NCO
            const float omega = 2.0f * FLOAT_PI * cw_fill_msg_current.freq /
                (float)cw_psk_samplerate;

            const int samples_per_symbol = (cw_fill_msg_current.dit_duration == -1) ?
                /* BPSK31 is at 31.25 symbols per second. */
                cw_psk_samplerate * 100 / 3125 :
                (cw_fill_msg_current.dit_duration == -2) ?
                /* BPSK63 is at 2*31.25 symbols per second. */
                cw_psk_samplerate * 50 / 3125 :
                (cw_fill_msg_current.dit_duration == -3) ?
                /* BPSK125 is at 4*31.25 symbols per second. */
                cw_psk_samplerate * 25 / 3125 :
                /* CW directly depends on dit_duration, which is in ms */
                (cw_psk_samplerate * cw_fill_msg_current.dit_duration) / 1000;

            psk_current_psk_phase = 1;

            for (int i = 0; i < cw_psk_buffer_len; i++) {
                for (int t = 0; t < samples_per_symbol; t++) {
                    int16_t s = (cw_fill_msg_current.dit_duration > 0) ?
                        cw_generate_audio(omega, i, t) :
                        psk_generate_audio(omega, i, t, samples_per_symbol);

                    // Stereo
                    for (int channel = 0; channel < 2; channel++) {
                        if (buf_pos == AUDIO_BUF_LEN) {
                            // It should take AUDIO_BUF_LEN/cw_psk_samplerate seconds to send one buffer.
                            // If it takes more than 4 times as long, we think there is a problem.
                            const TickType_t reasonable_delay = pdMS_TO_TICKS(4000 * AUDIO_BUF_LEN / cw_psk_samplerate);
                            if (xQueueSendToBack(cw_audio_queue, &cw_audio_buf, reasonable_delay) != pdTRUE) {
                                trigger_fault(FAULT_SOURCE_CW_AUDIO_QUEUE);
                            }
                            buf_pos = 0;
                        }
                        cw_audio_buf[buf_pos++] = s;
                    }

                }

                if (cw_psk_buffer[i] == 0) {
                    psk_current_psk_phase *= -1;
                }
            }

            // Flush remaining audio buffer
            if (buf_pos < AUDIO_BUF_LEN) {
                while (buf_pos < AUDIO_BUF_LEN) {
                    cw_audio_buf[buf_pos++] = 0;
                }

                const TickType_t reasonable_delay = pdMS_TO_TICKS(4000 * AUDIO_BUF_LEN / cw_psk_samplerate);
                if (xQueueSendToBack(cw_audio_queue, &cw_audio_buf, reasonable_delay) != pdTRUE) {
                    trigger_fault(FAULT_SOURCE_CW_AUDIO_QUEUE);
                }
            }
            buf_pos = 0;

            // We have completed this message
            cw_transmit_ongoing = 0;
        }
    }
}

int cw_psk_busy(void)
{
    return cw_transmit_ongoing;
}

