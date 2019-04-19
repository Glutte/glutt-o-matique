/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Maximilien Cuony, Matthias P. Braendli
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

#include "Audio/tone.h"
#include "Core/common.h"
#include "GPIO/usart.h"

#include <stdlib.h>
#include "queue.h"

#ifdef SIMULATOR
#include <math.h>
#define arm_cos_f32 cosf
#define arm_sin_f32 sinf
#else
#include "arm_math.h"
#endif

#define TONE_N 800

/* DTMF frequencies
 *      COL_1  COL_2  COL_3  COL_A
 *      1209   1336   1477   1633
 * 697  [1]    [2]    [3]    [A]      ROW_1
 * 770  [4]    [5]    [6]    [B]      ROW_4
 * 852  [7]    [8]    [9]    [C]      ROW_7
 * 941  [*]    [0]    [#]    [D]      ROW_STAR
 *
 * We need 0, 7, and 1750Hz for now, having detectors for
 * 1209, 1336, 852, 941 and 1750 is sufficient. */

#define DET_COL_1 0
#define DET_COL_2 1
#define DET_ROW_7 2
#define DET_ROW_STAR 3
#define DET_1750 4
#define NUM_DETECTORS 5

// Incomplete because not all frequencies decoded
enum dtmf_code {
    DTMF_NONE = 0,
    DTMF_0,
    DTMF_7,
    DTMF_8,
    DTMF_STAR,
};

struct tone_detector {
    float coef;
    float Q1;
    float Q2;

    int num_samples_analysed;
};

static struct tone_detector detectors[NUM_DETECTORS];

static int num_samples_analysed = 0;
static int lost_results = 0;
static QueueHandle_t m_squared_queue;

// Apply an IIR filter with alpha = 5/16 to smooth out variations.
// Values are normalised s.t. the mean corresponds to 100
static int32_t normalised_results[NUM_DETECTORS];

static int tone_1750_detected = 0;
static int detectors_enabled = 0;

static enum dtmf_code dtmf_last_seen = 0;
static uint64_t dtmf_last_seen_at = 0;

// Store the sequence of dtmf codes in this FIFO. If no DTMF code gets
// decoded in the interval, a NONE gets inserted into the sequence
#define NUM_DTMF_SEQ 3
#define DTMF_MAX_TONE_INTERVAL 1500
static enum dtmf_code dtmf_sequence[NUM_DTMF_SEQ];

static char* dtmf_to_str(enum dtmf_code code)
{
    char *codestr;
    switch (code) {
        case DTMF_0: codestr = "0"; break;
        case DTMF_7: codestr = "7"; break;
        case DTMF_8: codestr = "8"; break;
        case DTMF_STAR: codestr = "*"; break;
        case DTMF_NONE: codestr = "x"; break;
    }
    return codestr;
}

static inline void push_dtmf_code(enum dtmf_code code)
{
    for (int i = 0; i < NUM_DTMF_SEQ-1; i++) {
        dtmf_sequence[i] = dtmf_sequence[i+1];
    }
    dtmf_sequence[NUM_DTMF_SEQ-1] = code;

    usart_debug("DTMF: [%s, %s, %s]\r\n",
            dtmf_to_str(dtmf_sequence[0]),
            dtmf_to_str(dtmf_sequence[1]),
            dtmf_to_str(dtmf_sequence[2]));
}

// TODO: Does that depend on TONE_N?
const int thresh_dtmf = 200;
const int thresh_1750 = 200;

static void analyse_dtmf()
{
    // Bits 0 to 9 are numbers, bit 10 to 13 letters, bit 14 is star, 15 is hash
    const uint16_t pattern =
        ((normalised_results[DET_COL_1] > thresh_dtmf &&
          normalised_results[DET_ROW_7] > thresh_dtmf) ? (1 << 7) : 0) +
        ((normalised_results[DET_COL_2] > thresh_dtmf &&
          normalised_results[DET_ROW_7] > thresh_dtmf) ? (1 << 8) : 0) +
        ((normalised_results[DET_COL_1] > thresh_dtmf &&
          normalised_results[DET_ROW_STAR] > thresh_dtmf) ? (1 << 14) : 0) +
        ((normalised_results[DET_COL_2] > thresh_dtmf &&
          normalised_results[DET_ROW_STAR] > thresh_dtmf) ? (1 << 0) : 0);

    // Match patterns exactly to exclude multiple simultaneous DTMF codes.
    if (pattern == (1 << 0)) {
        if (dtmf_last_seen != DTMF_0) {
            push_dtmf_code(DTMF_0);
        }

        dtmf_last_seen = DTMF_0;
        dtmf_last_seen_at = timestamp_now();
    }
    else if (pattern == (1 << 7)) {
        if (dtmf_last_seen != DTMF_7) {
            push_dtmf_code(DTMF_7);
        }

        dtmf_last_seen = DTMF_7;
        dtmf_last_seen_at = timestamp_now();
    }
    else if (pattern == (1 << 8)) {
        if (dtmf_last_seen != DTMF_8) {
            push_dtmf_code(DTMF_8);
        }

        dtmf_last_seen = DTMF_8;
        dtmf_last_seen_at = timestamp_now();
    }
    else if (pattern == (1 << 14)) {
        if (dtmf_last_seen != DTMF_STAR) {
            push_dtmf_code(DTMF_STAR);
        }

        dtmf_last_seen = DTMF_STAR;
        dtmf_last_seen_at = timestamp_now();
    }
    else if (dtmf_last_seen_at + DTMF_MAX_TONE_INTERVAL < timestamp_now()) {
        // Flush out all codes
        push_dtmf_code(DTMF_NONE);

        dtmf_last_seen = DTMF_NONE;
        dtmf_last_seen_at = timestamp_now();
    }
}


int tone_1750_status()
{
    return tone_1750_detected;
}

int tone_fax_status()
{
    return dtmf_sequence[0] == DTMF_0 &&
           dtmf_sequence[1] == DTMF_7 &&
           dtmf_sequence[2] == DTMF_STAR;
}

static inline void init_detector(struct tone_detector* detector, int freq) {
    detector->coef = 2.0f * arm_cos_f32(2.0f * FLOAT_PI * freq / AUDIO_IN_RATE);
    detector->Q1 = 0;
    detector->Q2 = 0;
    detector->num_samples_analysed = 0;
}

void tone_init() {
    m_squared_queue = xQueueCreate(2, NUM_DETECTORS * sizeof(float));
    if (m_squared_queue == 0) {
        trigger_fault(FAULT_SOURCE_ADC2_QUEUE);
    }

    for (int i = 0; i < NUM_DTMF_SEQ; i++) {
        dtmf_sequence[i] = DTMF_NONE;
    }

    init_detector(&detectors[DET_COL_1], 1209);
    init_detector(&detectors[DET_COL_2], 1336);
    init_detector(&detectors[DET_ROW_7], 852);
    init_detector(&detectors[DET_ROW_STAR], 941);
    init_detector(&detectors[DET_1750], 1750);
}

void tone_detector_enable(int enable)
{
    if (enable && !detectors_enabled) {
        for (int det = 0; det < NUM_DETECTORS; det++) {
            detectors[det].Q1 = 0;
            detectors[det].Q2 = 0;
            detectors[det].num_samples_analysed = 0;
        }
        audio_in_enable(1);
        detectors_enabled = 1;
    }
    else if (!enable && detectors_enabled) {
        audio_in_enable(0);
        detectors_enabled = 0;
    }
}

/* Analyse a sample. Returns -1 if more samples needed, 0 if no tone detected,
 * 1 if a tone was detected.
 */
static inline float analyse_sample(int16_t sample, struct tone_detector *detector)
{
    float Q0 = detector->coef * detector->Q1 - detector->Q2 + sample;
    detector->Q2 = detector->Q1;
    detector->Q1 = Q0;

    detector->num_samples_analysed++;

    if (detector->num_samples_analysed == TONE_N) {
        const float m = sqrtf(
                detector->Q1 * detector->Q1 +
                detector->Q2 * detector->Q2 -
                detector->coef * detector->Q1 * detector->Q2);

        detector->Q1 = 0;
        detector->Q2 = 0;
        detector->num_samples_analysed = 0;

        return m;
    }
    else {
        return 0;
    }
}

void tone_detect_push_sample_from_irq(const int16_t sample)
{
    for (int det = 0; det < NUM_DETECTORS; det++) {
        struct tone_detector *detector = &detectors[det];
        float Q0 = detector->coef * detector->Q1 - detector->Q2 + sample;
        detector->Q2 = detector->Q1;
        detector->Q1 = Q0;
    }
    num_samples_analysed++;

    if (num_samples_analysed == TONE_N) {
        float m_squared[NUM_DETECTORS];
        for (int det = 0; det < NUM_DETECTORS; det++) {
            struct tone_detector *detector = &detectors[det];
            m_squared[det] =
                detector->Q1 * detector->Q1 +
                detector->Q2 * detector->Q2 -
                detector->coef * detector->Q1 * detector->Q2;
            detector->Q1 = 0;
            detector->Q2 = 0;
        }

        BaseType_t require_context_switch = 0;
        int success = xQueueSendToBackFromISR(
                m_squared_queue,
                &m_squared,
                &require_context_switch);

        if (success == pdFALSE) {
            lost_results++;
        }
    }
}

void tone_do_analysis()
{
    float m[NUM_DETECTORS];
    while (!xQueueReceive(m_squared_queue, &m, portMAX_DELAY)) {}

    float inv_mean = 0;
    for (int det = 0; det < NUM_DETECTORS; det++) {
        m[det] = sqrtf(m[det]);
        inv_mean += m[det];
    }
    inv_mean = NUM_DETECTORS / inv_mean;

    for (int det = 0; det < NUM_DETECTORS; det++) {
        normalised_results[det] =
            (11 * normalised_results[det] +
             (int)(5 * 100 * m[det] * inv_mean))
            >> 4; // divide by 16
    }

    tone_1750_detected = (normalised_results[DET_1750] > thresh_1750);
    analyse_dtmf();

    static int printcounter = 0;
    if (++printcounter == 5) {
        usart_debug("Tones: % 3d % 3d % 3d % 3d % 3d\r\n",
                normalised_results[0],
                normalised_results[1],
                normalised_results[2],
                normalised_results[3],
                normalised_results[4]);

        printcounter = 0;
    }
}

#if 0
void tone_detect_push_samples(const int16_t *samples, int len)
{
    int32_t mean = 0;
    int32_t min = 0x7fffffff;
    int32_t max = -0x7fffffff;
    for (int i = 0; i < len; i++) {
        const int16_t s = samples[i];
        mean += s;
        if (s < min) {
            min = s;
        }
        if (s > max) {
            max = s;
        }
    }
    mean /= len;

    max -= mean;
    min -= mean;

    /* min and max should now be more or less opposite, assuming the
     * signal is symmetric around its mean. Take the larger of the two,
     * for the scalefactor calculation. */
    const int32_t dev = (max > -min) ? max : -min;

    /* Scale the signal [min - max] -> [-2^15, 2^15] by doing
     * dev -> 2^15
     */
    const int32_t scalefactor = (1<<15) / dev;

    float results[NUM_DETECTORS];
    for (int det = 0; det < NUM_DETECTORS; det++) {
        results[det] = 0;
    }

    for (int i = 0; i < len; i++) {
        const int16_t s = scalefactor * (samples[i] - mean);

        for (int det = 0; det < NUM_DETECTORS; det++) {
            results[det] += analyse_sample(s, &detectors[det]);
        }
    }

    /* Normalise the results so that a relative comparison of
     * the different detectors can be done. This is more reliable
     * than looking at the detector outputs independently, especially
     * in the presence of noise whose amplitude varies. */
    float inv_mean = 0;
    for (int det = 0; det < NUM_DETECTORS; det++) {
        inv_mean += results[det];
    }
    inv_mean = NUM_DETECTORS / inv_mean;

    for (int det = 0; det < NUM_DETECTORS; det++) {
        normalised_results[det] =
            (11 * normalised_results[det] +
             (int)(5 * 100 * results[det] * inv_mean))
            >> 4; // divide by 16
    }

    tone_1750_detected = (normalised_results[DET_1750] > thresh_1750);
    analyse_dtmf();

    static int printcounter = 0;
    if (++printcounter == 5) {
        usart_debug("Tones: % 3d % 3d % 3d % 3d % 3d\r\n",
                normalised_results[0],
                normalised_results[1],
                normalised_results[2],
                normalised_results[3],
                normalised_results[4]);

        printcounter = 0;
    }
}
#endif
