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

#ifdef SIMULATOR
#include <math.h>
#define arm_cos_f32 cosf
#define arm_sin_f32 sinf
#else
#include "arm_math.h"
#endif

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
static struct tone_detector detectors[NUM_DETECTORS];

// Apply an IIR filter with alpha = 5/16 to smooth out variations.
// Values are normalised s.t. the mean corresponds to 100
static int32_t normalised_results[NUM_DETECTORS];

static int tone_1750_detected = 0;


int tone_1750_status()
{
    return tone_1750_detected;
}

static void init_tone(struct tone_detector* detector, int freq) {
    detector->coef = 2.0f * arm_cos_f32(2.0f * FLOAT_PI * freq / AUDIO_IN_RATE);
    detector->Q1 = 0;
    detector->Q2 = 0;
    detector->num_samples_analysed = 0;
}

void tone_init() {
    init_tone(&detectors[DET_COL_1], 1209);
    init_tone(&detectors[DET_COL_2], 1336);
    init_tone(&detectors[DET_ROW_7], 852);
    init_tone(&detectors[DET_ROW_STAR], 941);
    init_tone(&detectors[DET_1750], 1750);
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

void tone_detect_1750(const int16_t *samples, int len)
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

    tone_1750_detected = (normalised_results[DET_1750] > 400);

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

