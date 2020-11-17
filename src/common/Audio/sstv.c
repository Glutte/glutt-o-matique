/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Maximilien Cuony
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

/* SSTV Generator
 * Concept:
 *
 * [TODO]
 *
 * Use the cw_audio_queue to push sample into audio buffer. This will be done only if the CW/PSK31 tasks are not busy
 **/

#include "Audio/sstv.h"
#include "Audio/cw.h"
#include "Core/common.h"
#include "Audio/audio.h"
#include <string.h>

#ifdef SIMULATOR
#include <stdio.h>
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

struct sstv_tick_s {
    int           freq;
    uint64_t      duration_ns;
};

struct sstv_reader_action {
    int pos;
    int length;
};

// The queue contains above structs
QueueHandle_t sstv_msg_queue;

extern QueueHandle_t cw_audio_queue;

static int16_t sstv_audio_buf[AUDIO_BUF_LEN];
static struct sstv_reader_action sstv_fill_msg_current;

static int    sstv_samplerate;
static int    sstv_transmit_ongoing;

static void sstv_task(void *pvParameters);


#define SSTV_BUFFER_NS 100000000uL
#define SSTV_BUFFER_OUT_SIZE (222 * 4)  // At 16000, ~22 max element per message if we sent it every 10ms. At 66 we have [22 elements written, 22 elements in queue, 22 elements read] at most


static struct sstv_tick_s sstv_out_circular_buffer[SSTV_BUFFER_OUT_SIZE];
static int sstv_circular_buffer_out = 0;
static int sstv_circular_buffer_last_out = 0;
static int sstv_circular_buffer_out_size = 0;
static uint64_t sstv_circular_buffer_last_duration_ns = 0;


void sstv_init(unsigned int samplerate) {

    sstv_samplerate = samplerate;
    sstv_transmit_ongoing = 0;

    sstv_msg_queue = xQueueCreate(1, sizeof(struct sstv_reader_action));
    if (sstv_msg_queue == 0) {
        while(1); /* fatal error */
    }

    xTaskCreate(
            sstv_task,
            "SSTVTask",
            8*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 3UL,
            NULL);
}

static void sstv_send(int f, uint64_t duration_ns) {
    sstv_out_circular_buffer[sstv_circular_buffer_out].freq = f;
    sstv_out_circular_buffer[sstv_circular_buffer_out].duration_ns = duration_ns;

    sstv_circular_buffer_out++;
    sstv_circular_buffer_out_size++;
    sstv_circular_buffer_last_duration_ns += duration_ns;

    if (sstv_circular_buffer_out == SSTV_BUFFER_OUT_SIZE) {
        sstv_circular_buffer_out = 0;
    }

    if (sstv_circular_buffer_last_duration_ns > SSTV_BUFFER_NS) {
        struct sstv_reader_action msg;
        msg.pos = sstv_circular_buffer_last_out;
        msg.length = sstv_circular_buffer_out_size;

        sstv_circular_buffer_last_out = (sstv_circular_buffer_last_out + sstv_circular_buffer_out_size) % SSTV_BUFFER_OUT_SIZE;
        sstv_circular_buffer_out_size = 0;
        sstv_circular_buffer_last_duration_ns = 0;

        printf("SEND %li Pos=%i Length=%i\n", xQueueSendToBack(sstv_msg_queue, &msg, portMAX_DELAY), msg.pos, msg.length); /* Send Message */
    } else {

        /* printf("Not send, at %ld (Pos=%i, OutSize=%i)\n", sstv_circular_buffer_last_duration_ns, sstv_circular_buffer_out, sstv_circular_buffer_out_size); */
    }

}

static void sstv_send_ms(int f, uint64_t duration_ms) {
    return sstv_send(f, duration_ms * 1000000);
}

static float luminance(int line, int row) {
    float l = 1500;

    if (line < 64) {
        const int line_reduced = line / 4;
        const int row_reduced = row / 4;

        l = ((line_reduced % 2) ^ (row_reduced % 2)) ? 1500 : 2300;
    }
    else if (line < 128) {
        // Map line 128 to 0, line 192 to (2300 - 1500) = 800
        float offs = (line - 64) * (800.0f / 64.0f);
        l = ((row % 8) >= 4) ? 1500 + offs : 2300 - offs;
    }
    else if (line < 192) {
        l = ((row % 8) < 4) ? 1500 : 2400;
    }
    else {
        l = ((row % 4) >= 2) ? 1600 : 2300;
    }

    return l;
}

void sstv_test(void) {

    sstv_send_ms(1500, 1000);

    sstv_send_ms(1900, 300);
    sstv_send_ms(1200, 30);
    sstv_send_ms(1900, 300);

    sstv_send_ms(1200, 30); // Start
    sstv_send_ms(1300, 30); // 0
    sstv_send_ms(1300, 30); // 1
    sstv_send_ms(1100, 30); // 2
    sstv_send_ms(1100, 30); // 3
    sstv_send_ms(1300, 30); // 4
    sstv_send_ms(1100, 30); // 5
    sstv_send_ms(1300, 30); // 6
    sstv_send_ms(1100, 30); // P
    sstv_send_ms(1200, 30); // Stop

    const uint64_t SYNC_PULSE = 4862000;
    const uint64_t SYNC_PORCH = 572000;
    const uint32_t PIXELS_PER_LINE = 320;
    const uint64_t PIXEL = 457600;

    // Luminance: 1500 Hz to 2300 Hz

    for (int line = 0; line < 256; line++) {
        // Sync pulse + porch
        sstv_send(1200, SYNC_PULSE);
        sstv_send(1500, SYNC_PORCH);

        // Green scan + separator pulse
        for (int j = 0; j < PIXELS_PER_LINE; j++) {
            sstv_send(luminance(line, j), PIXEL);
        }
        sstv_send(1500, SYNC_PORCH);

        // Blue scan + separator pulse
        for (int j = 0; j < PIXELS_PER_LINE; j++) {
            sstv_send(luminance(line, j), PIXEL);
        }
        sstv_send(1500, SYNC_PORCH);

        // Red scan + separator pulse
        for (int j = 0; j < PIXELS_PER_LINE; j++) {
            sstv_send(luminance(line, j), PIXEL);
        }
        sstv_send(1500, SYNC_PORCH);
    }

    sstv_send(0, 0);
}


static float sstv_generate_audio_ampl = 0.0f;
static float sstv_generate_audio_nco = 0.0f;
static int16_t sstv_generate_audio(float omega)
{
    sstv_generate_audio_ampl = 32768.0f;

    sstv_generate_audio_nco += omega;
    if (sstv_generate_audio_nco > FLOAT_PI) {
        sstv_generate_audio_nco -= 2.0f * FLOAT_PI;
    }

    int16_t s = sstv_generate_audio_ampl * arm_sin_f32(sstv_generate_audio_nco);
    return s;
}

static void sstv_task(void __attribute__ ((unused))*pvParameters)
{
    int     buf_pos = 0;

    while (1) {
        int status = xQueueReceive(sstv_msg_queue, &sstv_fill_msg_current, portMAX_DELAY);

        if (status == pdTRUE) {

            sstv_transmit_ongoing = 1;

            uint64_t current_nano = 0;
            const uint64_t nano_per_sample = 1000000000uL / (uint64_t)sstv_samplerate;

            bool stop = 0;

            int current_position = sstv_fill_msg_current.pos;
            int remaining = sstv_fill_msg_current.length;

            float omega = 2.0f * FLOAT_PI * (float)(sstv_out_circular_buffer[current_position].freq) / (float)sstv_samplerate;

            int g = 0;

            printf("Starting (Pos=%i, Remainting=%i, Duration=%ld)\n", current_position, remaining, sstv_out_circular_buffer[current_position].duration_ns);

            while(!stop) {

                // Angular frequency of NCO

                int16_t s = sstv_generate_audio(omega);
                /* printf("%i ", s); */

                // Stereo
                for (int channel = 0; channel < 2; channel++) {
                    if (buf_pos == AUDIO_BUF_LEN) {
                        printf("Outting buffer\n");
                        // It should take AUDIO_BUF_LEN/sstv_samplerate seconds to send one buffer.
                        // If it takes more than 4 times as long, we think there is a problem.
                        const TickType_t reasonable_delay = pdMS_TO_TICKS(4000 * AUDIO_BUF_LEN / sstv_samplerate);
                        if (xQueueSendToBack(cw_audio_queue, &sstv_audio_buf, reasonable_delay) != pdTRUE) {
                            trigger_fault(FAULT_SOURCE_CW_AUDIO_QUEUE);
                        }
                        buf_pos = 0;
                    }
                    sstv_audio_buf[buf_pos++] = s;
                }
                g++;

                current_nano += nano_per_sample;

                if (current_nano >= sstv_out_circular_buffer[current_position].duration_ns) {
                    /* printf("Message took %ld / %ld, %i samples pushed %f\n", current_nano, sstv_out_circular_buffer[current_position].duration_ns, g, nano_per_sample); */
                    g = 0;
                    current_nano -= sstv_out_circular_buffer[current_position].duration_ns;

                    current_position = (current_position + 1) % SSTV_BUFFER_OUT_SIZE;
                    remaining--;

                    if (remaining == 0) {
                        /* printf("Remaining = 0, reading \n"); */

                        printf("READ %li \n ", xQueueReceive(sstv_msg_queue, &sstv_fill_msg_current, portMAX_DELAY));

                        if (sstv_fill_msg_current.length == 0) {
                            stop = 1;
                        } else {
                            current_position = sstv_fill_msg_current.pos;
                            remaining = sstv_fill_msg_current.length;
                            /* printf("Result: Remaining = %i, continue to pos %i \n", remaining, current_position); */
                        }

                    } else {
                        /* printf("Remaining = %i, continue to pos %i \n", remaining, current_position); */
                    }

                    omega = 2.0f * FLOAT_PI * (float)(sstv_out_circular_buffer[current_position].freq) / (float)sstv_samplerate;
                }

            }


            // Flush remaining audio buffer
            if (buf_pos < AUDIO_BUF_LEN) {
                while (buf_pos < AUDIO_BUF_LEN) {
                    sstv_audio_buf[buf_pos++] = 0;
                }

                const TickType_t reasonable_delay = pdMS_TO_TICKS(4000 * AUDIO_BUF_LEN / sstv_samplerate);
                if (xQueueSendToBack(cw_audio_queue, &sstv_audio_buf, reasonable_delay) != pdTRUE) {
                    trigger_fault(FAULT_SOURCE_CW_AUDIO_QUEUE);
                }
            }
            buf_pos = 0;


            // We have completed this message
            sstv_transmit_ongoing = 0;
        }
    }
}
