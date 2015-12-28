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

    cw_msg_queue = xQueueCreate(15, sizeof(struct cw_out_message_s));
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

    // silence(2)
    if (msg->on_buffer_end + 2 < ON_BUFFER_SIZE) {
        for (int i = 0; i < 2; i++) {
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
            if (msg.on_buffer_end + 3 < ON_BUFFER_SIZE) {
                for (int i = 0; i < 3; i++) {
                    msg.on_buffer[msg.on_buffer_end++] = 0;
                }
            }
        }
        else {
            cw_symbol(*sym - '+', &msg);
        }
        sym++;
    } while (*sym != '\0');

    xQueueSendToBack(cw_msg_queue, &msg, portMAX_DELAY); /* Send Message */
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
static void cw_task(void *pvParameters)
{
    struct cw_out_message_s cw_fill_msg_current;

    float nco_phase = 0.0f;
    float ampl = 0.0f;

    int     buf_pos = 0;

    while (1) {
        int status = xQueueReceive(cw_msg_queue, &cw_fill_msg_current, portMAX_DELAY);
        if (status == pdTRUE) {

            cw_transmit_ongoing = 1;

            const int samples_per_dit =
                (cw_samplerate * cw_fill_msg_current.dit_duration) / 1000;

            // Angular frequency of NCO
            const float omega = 2.0f * FLOAT_PI * cw_fill_msg_current.freq /
                (float)cw_samplerate;

            for (int i = 0; i < cw_fill_msg_current.on_buffer_end; i++) {
                for (int t = 0; t < samples_per_dit; t++) {
                    int16_t s = 0;

                    // Remove clicks from CW
                    if (cw_fill_msg_current.on_buffer[i]) {
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

