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

#include <stdio.h>
#include <pulse/simple.h>
#include "Audio/audio.h"
#include "FreeRTOS.h"
#include "task.h"

pa_simple *s = NULL;

int current_buffer_length = 0;
int16_t * current_buffer_samples;

static void audio_buffer_sent(void);
static void audio_buffer_sender(void *args);

extern char gui_audio_on;

void audio_initialize_platform(int __attribute__ ((unused))plln, int __attribute__ ((unused))pllr, int __attribute__ ((unused))i2sdiv, int __attribute__ ((unused))i2sodd, int rate) {

    int error;

    static pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 0,
        .channels = 2
    };

    ss.rate = rate;

    s = pa_simple_new(NULL, "Glutte", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error);

    if (s) {

    } else {
        printf("Pulseaudio playback init error\n");
        while(1);
    }


    TaskHandle_t task_handle;
    xTaskCreate(
            audio_buffer_sender,
            "Audio buffer sender",
            configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            &task_handle);

}

static void audio_buffer_sender(void __attribute__ ((unused)) *args) {

    while(1) {

        if (current_buffer_length != 0) {
            pa_simple_write(s, current_buffer_samples, current_buffer_length * 2, NULL);
            current_buffer_length = 0;
            audio_buffer_sent();
        }

        taskYIELD();
    }

}

static void audio_buffer_sent() {

    if (next_buffer_samples) {
        audio_start_dma_and_request_buffers();
    } else {
        dma_running = false;
    }

}

void audio_on() {
    gui_audio_on = 1;
}

void audio_off() {
    gui_audio_on = 0;
}

void audio_set_volume(int __attribute__ ((unused))volume) {
}


void audio_output_sample_without_blocking(int16_t __attribute__ ((unused))sample) {
}

void audio_play_with_callback(AudioCallbackFunction *callback, void *context) {
    audio_stop_dma();

    callback_function = callback;
    callback_context = context;
    buffer_number = 0;

    if (callback_function)
        callback_function(callback_context, buffer_number);
}

void audio_stop() {
    audio_stop_dma();
    callback_function = NULL;
}

void audio_provide_buffer(void *samples, int numsamples) {
    while (!audio_provide_buffer_without_blocking(samples, numsamples))
        __asm__ volatile ("nop");
}

bool audio_provide_buffer_without_blocking(void *samples, int numsamples) {
    if (next_buffer_samples)
        return false;

    next_buffer_samples = samples;
    next_buffer_length = numsamples;

    if (!dma_running)
        audio_start_dma_and_request_buffers();

    return true;
}

void audio_start_dma_and_request_buffers() {

    current_buffer_length = next_buffer_length;
    current_buffer_samples = next_buffer_samples;

    // Update state.
    next_buffer_samples = NULL;
    buffer_number ^= 1;
    dma_running = true;

    // Invoke callback if it exists to queue up another buffer.
    if (callback_function)
        callback_function(callback_context, buffer_number);
}

void audio_stop_dma() {
    dma_running = false;
}

