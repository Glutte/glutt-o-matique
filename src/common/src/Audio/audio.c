/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Matthias P. Braendli, Maximilien Cuony
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

#include "Audio/audio.h"

#include <stdlib.h>

static void audio_write_register(uint8_t address, uint8_t value);
static void audio_start_dma_and_request_buffers();
static void audio_stop_dma();

static AudioCallbackFunction *callback_function;
static void *callback_context;
static int16_t * volatile next_buffer_samples;
static volatile int next_buffer_length;
static volatile int buffer_number;
static volatile bool dma_running;

void audio_initialize_platform(int plln, int pllr, int i2sdiv, int i2sodd, int rate);

void audio_initialize(int plln, int pllr, int i2sdiv, int i2sodd, int rate) {

    // Intitialize state.
    callback_function = NULL;
    callback_context = NULL;
    next_buffer_samples = NULL;
    next_buffer_length = 0;
    buffer_number = 0;
    dma_running = false;

    audio_initialize_platform(plln, pllr, i2sdiv, i2sodd, rate);

    audio_set_volume(0xff);

}
