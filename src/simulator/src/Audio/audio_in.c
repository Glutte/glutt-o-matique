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

#include <stdio.h>
#include <assert.h>
#include <pulse/simple.h>
#include "Audio/audio_in.h"
#include "Audio/tone.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

pa_simple *s_in = NULL;

static int16_t buffers[2][AUDIO_IN_BUF_LEN];
static int current_buffer = 0;

static int enabled = 0; // TODO concurrent access: must be protected by a mutex :-/

static QueueHandle_t adc2_values_queue;

static void audio_buffer_reader(void __attribute__((unused))*args)
{
    while (1) {
        pa_simple_read(s_in, buffers[current_buffer], AUDIO_IN_BUF_LEN * sizeof(int16_t), NULL);

        if (enabled) {
            int success = xQueueSendToBack(
                    adc2_values_queue,
                    &current_buffer,
                    portMAX_DELAY);
            assert(success);
        }

        current_buffer = (current_buffer + 1) % 2;

        taskYIELD();
    }
}


void audio_in_initialize() {
    int error;

    static pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 0,
        .channels = 1
    };

    ss.rate = AUDIO_IN_RATE;

    s_in = pa_simple_new(NULL, "GlutteR", PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error);
    assert(s_in);

    adc2_values_queue = xQueueCreate(4, sizeof(current_buffer));
    assert(adc2_values_queue);

    TaskHandle_t task_handle;
    xTaskCreate(
            audio_buffer_reader,
            "Audio buffer reader",
            configMINIMAL_STACK_SIZE,
            (void*) NULL,
            tskIDLE_PRIORITY + 2UL,
            &task_handle);
}

void audio_in_enable(int enable)
{
    enabled = enable;
}

int32_t audio_in_get_buffer(int16_t **buffer /*of length AUDIO_IN_BUF_LEN*/ )
{
    int last_written_buffer = 0;
    while (!xQueueReceive(adc2_values_queue, &last_written_buffer, portMAX_DELAY)) {}

    *buffer = buffers[last_written_buffer];

    return 0;
}

