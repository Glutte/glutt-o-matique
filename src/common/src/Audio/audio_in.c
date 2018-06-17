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
#include "Audio/audio_in.h"

int16_t audio_in_buffer_0[AUDIO_IN_BUF_LEN];
int16_t audio_in_buffer_1[AUDIO_IN_BUF_LEN];

void audio_in_initialize(int rate) {
    audio_in_buffer = audio_in_buffer_0;
    audio_in_initialize_plateform(rate);
}

void audio_in_buffer_ready() {

    for (int i = 0; i < AUDIO_IN_BUF_LEN; i++) {
        tone_detect_1750(audio_in_buffer[i]);
    }

    if (audio_in_buffer == audio_in_buffer_0) {
        audio_in_buffer = audio_in_buffer_1;
    } else {
        audio_in_buffer = audio_in_buffer_0;
    }

}
