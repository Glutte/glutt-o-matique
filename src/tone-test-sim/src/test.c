/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Maximilien Cuony, Matthias P. Braendli
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
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "vc.h"
#include "Audio/tone.h"

#define FLOAT_PI 3.1415926535897932384f

int main(int argc, char **argv)
{
    printf("Hello, ver %s\n", vc_get_version());
    printf("Saving results to tone.csv\n");
    printf("freq, threshold, num_samples, detector_output\n");

    FILE *fd = fopen("tone.csv", "w");
    if (!fd) {
        printf("Cannot create file\n");
        return 1;
    }

    const int freq_start = 1750 - 350;
    const int freq_stop = 1750 + 350;

    for (int freq = freq_start; freq < freq_stop; freq += 20) {
        for (int threshold = 200; threshold < 8000; threshold += 240) {
            tone_init(threshold);

            for (size_t j = 0; j < 200; j++) {
                float samplef = cosf(2.0f * FLOAT_PI * freq / AUDIO_IN_RATE);
                int16_t sample = samplef * 32767.0f;
                int r = tone_detect_1750(sample);
                if (r != -1) {
                    fprintf(fd, "%d,%d,%zu,%d\n",freq, threshold, j, r);
                }
            }
        }
    }
}
