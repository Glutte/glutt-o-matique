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
