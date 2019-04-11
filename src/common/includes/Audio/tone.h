/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Matthias P. Braendli, Maximilien Cuony
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

#ifndef __TONE_H_
#define __TONE_H_

#include <stdio.h>
#include "Audio/audio_in.h"

#define TONE_BUFFER_LEN AUDIO_IN_BUF_LEN

#define TONE_N 100

struct tone_detector {
    float coef;
    float Q1;
    float Q2;
    float threshold;

    int num_samples_analysed;
};

void tone_init(int threshold);

/* Return 1 when 1750 detected, 0 otherwise */
int tone_1750_status(void);

/* Update 1750 tone detection status */
void tone_detect_1750(const int16_t *samples, int len);

#endif