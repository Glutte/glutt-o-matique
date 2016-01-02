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

/* CW generator
 *
 * Concept:
 *
 * +-------------------+                    +-----------+
 * | cw_push_message() | -> cw_msg_queue -> | cw_task() | -> cw_audio_queue
 * +-------------------+                    +-----------+
 *
 * The cw_fill_buffer() function can be called to fetch audio from the audio_queue
 */

#include "cw.h"
#include "common.h"
#include "arm_math.h"
#include "audio.h"
#include "debug.h"

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

struct cw_message_s {
    char    message[MAX_MESSAGE_LEN];
    size_t  message_len;

    int     freq;
    int     dit_duration;
};

// The queue contains above structs
QueueHandle_t cw_msg_queue;

// Queue that contains audio data
QueueHandle_t cw_audio_queue;
static int    cw_samplerate;

static int    cw_transmit_ongoing;

static void cw_task(void *pvParameters);

void cw_init(unsigned int samplerate)
{
    cw_samplerate = samplerate;
    cw_transmit_ongoing = 0;

    cw_msg_queue = xQueueCreate(15, sizeof(struct cw_message_s));
    if (cw_msg_queue == 0) {
        while(1); /* fatal error */
    }

    cw_audio_queue = xQueueCreate(2, AUDIO_BUF_LEN * sizeof(int16_t));
    if (cw_audio_queue == 0) {
        while(1); /* fatal error */
    }

    xTaskCreate(
            cw_task,
            "TaskCW",
            8*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
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

// Transmit a string in morse code. Supported range:
// All ASCII between '+' and '\', which includes
// numerals and capital letters.
void cw_push_message(const char* text, int dit_duration, int frequency)
{
    const int text_len = strlen(text);

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

    xQueueSendToBack(cw_msg_queue, &msg, portMAX_DELAY); /* Send Message */
}

/* Parse the message and fill the on_buffer with CW on/CW off information.
 * Returns the number of on/off bits written.
 */
size_t cw_text_to_on_buffer(const char *msg, uint8_t *on_buffer, size_t on_buffer_size)
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

size_t cw_fill_buffer(int16_t *buf, size_t bufsize)
{
    if (xQueueReceiveFromISR(cw_audio_queue, buf, NULL)) {
        return bufsize;
    }
    else {
        return 0;
    }
}

static int16_t cw_audio_buf[AUDIO_BUF_LEN];
static uint8_t cw_on_buffer[MAX_ON_BUFFER_LEN];
static struct cw_message_s cw_fill_msg_current;
static void cw_task(void *pvParameters)
{
    float nco_phase = 0.0f;
    float ampl = 0.0f;

    int     buf_pos = 0;

    while (1) {
        int status = xQueueReceive(cw_msg_queue, &cw_fill_msg_current, portMAX_DELAY);
        if (status == pdTRUE) {

            size_t on_buffer_len = cw_text_to_on_buffer(
                    cw_fill_msg_current.message,
                    cw_on_buffer,
                    MAX_ON_BUFFER_LEN);

            cw_transmit_ongoing = 1;

            const int samples_per_dit =
                (cw_samplerate * cw_fill_msg_current.dit_duration) / 1000;

            // Angular frequency of NCO
            const float omega = 2.0f * FLOAT_PI * cw_fill_msg_current.freq /
                (float)cw_samplerate;

            for (int i = 0; i < on_buffer_len; i++) {
                for (int t = 0; t < samples_per_dit; t++) {
                    int16_t s = 0;

                    // Remove clicks from CW
                    if (cw_on_buffer[i]) {
                        const float remaining = 32768.0f - ampl;
                        ampl += remaining / 64.0f;
                    }
                    else {
                        ampl -= ampl / 64.0f;
                    }

                    nco_phase += omega;
                    if (nco_phase > FLOAT_PI) {
                        nco_phase -= 2.0f * FLOAT_PI;
                    }

                    s = ampl * arm_sin_f32(nco_phase);

                    if (buf_pos == AUDIO_BUF_LEN) {
                        xQueueSendToBack(cw_audio_queue, &cw_audio_buf, portMAX_DELAY);
                        buf_pos = 0;
                    }
                    cw_audio_buf[buf_pos++] = s;

                    // Stereo
                    if (buf_pos == AUDIO_BUF_LEN) {
                        xQueueSendToBack(cw_audio_queue, &cw_audio_buf, portMAX_DELAY);
                        buf_pos = 0;
                    }
                    cw_audio_buf[buf_pos++] = s;
                }
            }

            // We have completed this message

            cw_transmit_ongoing = 0;
        }
    }
}

int cw_busy(void)
{
    return cw_transmit_ongoing;
}

