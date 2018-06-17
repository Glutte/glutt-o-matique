/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Maximilien Cuony
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

#include <stdlib.h>

#ifdef SIMULATOR
#include <math.h>
#define arm_cos_f32 cosf
#define arm_sin_f32 sinf
#else
#include "arm_math.h"
#endif

static struct tone_detector detector_1750;

int TONE_1750_DETECTED = 0;

static void init_tone(struct tone_detector* detector, int freq, int threshold) {
    detector->coef = 2.0 * arm_cos_f32(2.0 * FLOAT_PI * freq / AUDIO_IN_RATE);
    detector->Q1 = 0;
    detector->Q2 = 0;
    detector->threshold = threshold ; // 200000;
    detector->num_samples_analysed = 0;
}

void tone_init(int threshold) {
    init_tone(&detector_1750, 1750, threshold);
}

/* Analyse a sample. Returns -1 if more samples needed, 0 if no tone detected,
 * 1 if a tone was detected.
 */
static inline int analyse_sample(int16_t sample, struct tone_detector *detector)
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

        if (m > detector->threshold) {
            return 1;
        }
        else {
            return 0;
        }
    }
    else {
        return -1;
    }
}

int tone_detect_1750(int16_t sample)
{
    int r = analyse_sample(sample, &detector_1750);
    if (r == 0 || r == 1) {
        TONE_1750_DETECTED = r;
    }
    return r;
}

