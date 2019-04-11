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
    printf("freq, threshold, num_samples, detector_output, avg_rms\n");

    FILE *fd = fopen("tone.csv", "w");
    if (!fd) {
        printf("Cannot create file\n");
        return 1;
    }

    const int freq_start = 1750 - 175;
    const int freq_stop = 1750 + 175;

    for (int freq = freq_start; freq < freq_stop; freq += 4) {
        for (int threshold = 100000; threshold < 4000000; threshold += 100000) {
            tone_init(threshold);

            double accu = 0;

            for (size_t j = 0; j < 100; j++) {
                const float samplef = cosf(j * 2.0f * FLOAT_PI * freq / AUDIO_IN_RATE);
                const float noisef = (drand48() * 2.0f) - 1;
                int16_t sample = (0.9f * samplef + 0.1f * noisef) * 32767.0f;

                accu += (sample * sample);

                int r = tone_detect_1750(sample);
                if (r != -1) {
                    const double rms = sqrt(accu / (double)j);
                    fprintf(fd, "%d,%d,%zu,%d,%f\n",freq, threshold, j, r, rms);
                }
            }
        }
    }
}
