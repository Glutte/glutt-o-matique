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

#ifdef SIMULATOR
#include <math.h>
#define arm_cos_f32 cosf
#define arm_sin_f32 sinf
#else
#include "arm_math.h"
#endif

static int current_pos = 0;


void init_tones() {
    init_tone(&detector_1750, 1750, 1);
}

void init_tone(struct tone_detector* detector, int freq, int threshold) {

    detector->coef = 2.0 * cos(2.0 * FLOAT_PI * freq / AUDIO_IN_RATE);
    detector->Q1 = 0;
    detector->Q2 = 0;
    detector->threshold = 200000;  // TODO

}

void detect_tones(int16_t * buffer) {

    // Normalize buffer
    int max_v = 0;
    for (int i = 0; i < TONE_BUFFER_LEN; i++) {
        max_v = fmax(abs(buffer[i]), max_v);
    }

    float coef = 32767.0 / max_v;

    for (int i = 0; i < TONE_BUFFER_LEN; i++) {
        buffer[i] *= coef;
    }

    TONE_1750_DETECTED = detect_tone(buffer, &detector_1750);
}

int detect_tone(int16_t * buffer, struct tone_detector* detector) {

    float Q0;
    int tt_samples = 0;
    int tt = 0;

    for (int i = 0; i < TONE_BUFFER_LEN; i++) {
        Q0 = detector->coef * detector->Q1 - detector->Q2 + buffer[i];
        detector->Q2 = detector->Q1;
        detector->Q1 = Q0;

        current_pos++;

        if (current_pos == TONE_N) {
            float m = sqrt(detector->Q1 * detector->Q1 + detector->Q2 * detector->Q2 - detector->coef * detector->Q1 * detector->Q2);

            if (m > detector->threshold) {
                tt += 1;
            }

            tt_samples++;
            detector->Q1 = 0;
            detector->Q2 = 0;
            current_pos = 0;
        }
    }

    return (float)tt / tt_samples > 0.5;
}
