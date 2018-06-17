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

    const int freq_start = 1750 - 175;
    const int freq_stop = 1750 + 175;

    for (int freq = freq_start; freq < freq_stop; freq += 20) {
        for (int threshold = 800; threshold < 8000; threshold += 80) {
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
