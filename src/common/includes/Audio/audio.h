#ifndef __AUDIO_H__
#define __AUDIO_H__

#include <stdint.h>
#include <stdbool.h>

typedef void AudioCallbackFunction(void *context,int buffer);

// Variables used by both glutt-o-logique and simulator
extern AudioCallbackFunction *callback_function;
extern void *callback_context;
extern int16_t *next_buffer_samples;
extern int next_buffer_length;
extern int buffer_number;
extern bool dma_running;

void audio_initialize_platform(int plln, int pllr, int i2sdiv, int i2sodd, int rate);
void audio_start_dma_and_request_buffers(void);
void audio_stop_dma(void);

#define Audio8000HzSettings 256,5,12,1,8000
#define Audio16000HzSettings 213,2,13,0,16000
#define Audio32000HzSettings 213,2,6,1,32000
#define Audio48000HzSettings 258,3,3,1,48000
#define Audio96000HzSettings 344,2,3,1,96000
#define Audio22050HzSettings 429,4,9,1,22050
#define Audio44100HzSettings 271,2,6,0,44100
#define AudioVGAHSyncSettings 419,2,13,0,31475 // 31475.3606. Actual VGA timer is 31472.4616.

#define AUDIO_BUF_LEN 4096


// Initialize and power up audio hardware. Use the above defines for the parameters.
// Can probably only be called once.
void audio_initialize(int plln,int pllr,int i2sdiv,int i2sodd, int rate);

// Power up and down the audio hardware.
void audio_put_codec_in_reset(void);
void audio_reinit_codec(void);

// Set audio volume in steps of 0.5 dB. 0xff is +12 dB.
void audio_set_volume(int volume);

// Start and stop audio playback using DMA.
// Callback is optional, and called whenever a new buffer is needed.
void audio_play_with_callback(AudioCallbackFunction *callback,void *context);

// Provide a new buffer to the audio DMA. Output is double buffered, so
// at least two buffers must be maintained by the program. It is not allowed
// to overwrite the previously provided buffer until after the next callback
// invocation.
// Buffers must reside in DMA1-accessible memory, that is, the 128k RAM bank,
// or flash.
bool audio_provide_buffer_without_blocking(void *samples,int numsamples);

void DMA1_Stream7_IRQHandler(void);

#endif
