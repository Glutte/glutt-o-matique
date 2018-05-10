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
};

static struct tone_detector detector_1750;


void init_tones(void);
void init_tone(struct tone_detector* detector, int freq, int threshold);
void detect_tones(int16_t * buffer);

int TONE_1750_DETECTED = 0;


#endif
