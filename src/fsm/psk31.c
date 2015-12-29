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

#include "psk31.h"
#include "common.h"
#include "audio.h"
#include <string.h>
#include "arm_math.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* PSK31 generator
 *
 * Concept:
 *
 * +----------------------+                       +--------------+
 * | psk31_push_message() | -> psk31_msg_queue -> | psk31_task() |
 * +----------------------+                       +--------------+
 *                                                       |
 *      _________________________________________________/
 *     /
 *     |
 *    \|/
 *     V
 *
 * psk31_audio_queue
 *
 * The psk31_fill_buffer() function can be called to fetch audio from the audio_queue
 */

/*
 * PSK31 Varicode
 * http://aintel.bi.ehu.es/psk31.html
 */
const char *psk31_varicode[] = { // {{{
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


#define PSK31_MAX_MESSAGE_LEN 4096
#define PHASE_BUFFER_SIZE (20 + PSK31_MAX_MESSAGE_LEN + 20)

struct psk31_out_message_s {
    // Contains a string sequence of '0' and '1' corresponding to
    // the BPSK31 phase. Is terminated with '\0'
    char    phase_buffer[PHASE_BUFFER_SIZE];
    size_t  phase_buffer_end;

    int     freq; // Audio frequency for signal center
};

// The queue contains above structs
QueueHandle_t psk31_msg_queue;

// Queue that contains audio data
QueueHandle_t psk31_audio_queue;
static int    psk31_samplerate;

static int    psk31_transmit_ongoing;

static void psk31_task(void *pvParameters);
static int psk31_str_to_bits(const char* instr, char* outbits);


void psk31_init(unsigned int samplerate)
{
    psk31_samplerate = samplerate;
    psk31_transmit_ongoing = 0;

    psk31_msg_queue = xQueueCreate(1, sizeof(struct psk31_out_message_s));
    if (psk31_msg_queue == 0) {
        while(1); /* fatal error */
    }

    psk31_audio_queue = xQueueCreate(1, AUDIO_BUF_LEN * sizeof(int16_t));
    if (psk31_audio_queue == 0) {
        while(1); /* fatal error */
    }

    xTaskCreate(
            psk31_task,
            "TaskPSK",
            5*configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            NULL);
}

int psk31_push_message(const char* text, int frequency)
{
    if (strlen(text) > PSK31_MAX_MESSAGE_LEN) {
        return 0;
    }

    struct psk31_out_message_s msg;
    msg.phase_buffer_end = 0;
    msg.freq = frequency;

    msg.phase_buffer_end = psk31_str_to_bits(text, msg.phase_buffer);

    xQueueSendToBack(psk31_msg_queue, &msg, portMAX_DELAY);

    return 1;
}

// Write the waveform into the buffer (stereo)
size_t psk31_fill_buffer(int16_t *buf, size_t bufsize)
{
    if (xQueueReceiveFromISR(psk31_audio_queue, buf, NULL)) {
        return bufsize;
    }
    else {
        return 0;
    }
}

int psk31_busy(void)
{
    return psk31_transmit_ongoing;
}

static int16_t psk31_audio_buf[AUDIO_BUF_LEN];
static struct psk31_out_message_s psk31_fill_msg_current;
static void psk31_task(void *pvParameters)
{
    float nco_phase = 0.0f;
    float ampl = 0.0f;

    int   buf_pos = 0;

    while (1) {
        int status = xQueueReceive(psk31_msg_queue, &psk31_fill_msg_current, portMAX_DELAY);
        if (status == pdTRUE) {

            psk31_transmit_ongoing = 1;

            const float base_ampl = 20000.0f;

            /* BPSK31 is at 31.25 symbols per second. */
            const int samples_per_symbol = psk31_samplerate * 100 / 3125;

            // Angular frequency of NCO
            const float omega = 2.0f * FLOAT_PI * psk31_fill_msg_current.freq /
                (float)psk31_samplerate;

            int current_psk_phase = 1;

            for (int i = 0; i < psk31_fill_msg_current.phase_buffer_end; i++) {
                for (int t = 0; t < samples_per_symbol; t++) {
                    int16_t s = 0;

                    if (psk31_fill_msg_current.phase_buffer[i] == '1') {
                        ampl = base_ampl;
                    }
                    else {
                        ampl = base_ampl * arm_cos_f32(
                                    FLOAT_PI*(float)t/(float)samples_per_symbol);
                    }

                    nco_phase += omega;
                    if (nco_phase > FLOAT_PI) {
                        nco_phase -= 2.0f * FLOAT_PI;
                    }

                    s = ampl * arm_sin_f32(nco_phase +
                            (current_psk_phase == 1 ? 0.0f : FLOAT_PI));

                    if (buf_pos == AUDIO_BUF_LEN) {
                        xQueueSendToBack(psk31_audio_queue, &psk31_audio_buf, portMAX_DELAY);
                        buf_pos = 0;
                    }
                    psk31_audio_buf[buf_pos++] = s;

                    // Stereo
                    if (buf_pos == AUDIO_BUF_LEN) {
                        xQueueSendToBack(psk31_audio_queue, &psk31_audio_buf, portMAX_DELAY);
                        buf_pos = 0;
                    }
                    psk31_audio_buf[buf_pos++] = s;
                }


                if (psk31_fill_msg_current.phase_buffer[i] == '0') {
                    current_psk_phase *= -1;
                }
            }

            // We have completed this message

            psk31_transmit_ongoing = 0;
        }
    }
}

/*
 * Turn a null terminated ASCII string into a null terminated
 * string of '0's and '1's representing the PSK31 varicode for the input.
 *
 * outstr must be at least size 20 + strlen(instr)*12 + 20 to accomodate
 * the header and tail
 */
static int psk31_str_to_bits(const char* instr, char* outbits)
{
    int i=0, j, k;

    /* Header of 0s */
    for (j=0; j < 20; j++) {
        outbits[i++] = '0';
    }

    /* Encode the message, with 00 between letters */
    for (j=0; j < strlen(instr); j++) {
        const char* varicode_bits = psk31_varicode[(int)instr[j]];
        for(k=0; k < strlen(varicode_bits); k++) {
            outbits[i++] = varicode_bits[k];
        }
        outbits[i++] = '0';
        outbits[i++] = '0';
    }

    /* Tail of 0s */
    for (j=0; j < 20; j++) {
        outbits[i++] = '0';
    }

    /* NULL terminate */
    outbits[i] = 0;
    return i;
}

#if 0
/*
 * Turn a null terminated string `bits` containing '0's and '1's
 * into `outlen` IQ samples for BPSK, `outbuf`.
 * Note that `outlen` is set to the number of IQ samples, i.e. half the
 * number of bytes in `outbuf`.
 * Allocates memory (possibly lots of memory) for the IQ samples, which
 * should be freed elsewhere.
 * Modulation:
 * '0': swap phase, smoothed by a cosine
 * '1': maintain phase
 * Output: I carries data, Q constantly 0
 */
void bits_to_iq(char* bits, uint8_t** outbuf, int* outlen)
{
    *outlen = strlen(bits) * 256000 * 2;
    *outbuf = malloc(*outlen);
    int8_t *buf = (int8_t*)(*outbuf);
    if(*outbuf == NULL) {
        fprintf(stderr, "Could not allocate memory for IQ buffer\n");
        exit(EXIT_FAILURE);
    }

    int i, j, phase = 1;
    for(i=0; i<strlen(bits); i++) {
        if(bits[i] == '1') {
            for(j=0; j<256000; j++) {
                buf[i*256000*2 + 2*j] = phase*50;
                buf[i*256000*2 + 2*j + 1] = 0;
            }
        } else {
            for(j=0; j<256000; j++) {
                buf[i*256000*2 + 2*j] = phase *
                    (int8_t)(50.0f * cosf(M_PI*(float)j/256000.0f));
                buf[i*256000*2 + 2*j + 1] = 0;
            }
            phase *= -1;
        }
    }
}
#endif

