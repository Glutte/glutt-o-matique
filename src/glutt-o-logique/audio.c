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

#include "GPIO/i2c.h"
#include "Audio/audio.h"
#include "stm32f4xx_conf.h"
#include "stm32f4xx.h"

const uint16_t CODEC_RESET_PIN = GPIO_Pin_4; // on GPIOD

static void audio_write_register(uint8_t address, uint8_t value);

void audio_initialize_platform(int plln, int pllr, int i2sdiv, int i2sodd, int __attribute__ ((unused)) rate) {

    GPIO_InitTypeDef  GPIO_InitStructure;

    // Turn on peripherals.
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);

    // Assume I2C is set up

    // Configure reset pin.
    GPIO_InitStructure.GPIO_Pin = CODEC_RESET_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // Configure I2S MCK, SCK, SD pins.
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_10 | GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_SPI3);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_SPI3);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource12, GPIO_AF_SPI3);

    // Configure I2S WS pin.
    GPIO_InitStructure.GPIO_Pin = CODEC_RESET_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource4, GPIO_AF_SPI3);

    audio_reinit_codec();

    // Disable I2S.
    SPI3 ->I2SCFGR = 0;

    // I2S clock configuration
    RCC ->CFGR &= ~RCC_CFGR_I2SSRC; // PLLI2S clock used as I2S clock source.
    RCC ->PLLI2SCFGR = (pllr << 28) | (plln << 6);

    // Enable PLLI2S and wait until it is ready.
    RCC ->CR |= RCC_CR_PLLI2SON;
    while (!(RCC ->CR & RCC_CR_PLLI2SRDY ))
        ;

    // Configure I2S.
    SPI3 ->I2SPR = i2sdiv | (i2sodd << 8) | SPI_I2SPR_MCKOE;
    SPI3 ->I2SCFGR = SPI_I2SCFGR_I2SMOD | SPI_I2SCFGR_I2SCFG_1
        | SPI_I2SCFGR_I2SE; // Master transmitter, Phillips mode, 16 bit values, clock polarity low, enable.

}

void audio_put_codec_in_reset(void) {
    GPIO_ResetBits(GPIOD, CODEC_RESET_PIN);
}

void audio_reinit_codec(void) {
    audio_put_codec_in_reset();
    for (volatile int i = 0; i < 0x4fff; i++) {
        __asm__ volatile("nop");
    }
    GPIO_SetBits(GPIOD, CODEC_RESET_PIN);

    // Configure codec.
    audio_write_register(0x02, 0x01); // Keep codec powered off.
    audio_write_register(0x04, 0xaf); // SPK always off and HP always on.

    audio_write_register(0x05, 0x81); // Clock configuration: Auto detection.
    audio_write_register(0x06, 0x04); // Set slave mode and Philips audio standard.

    // Power on the codec.
    audio_write_register(0x02, 0x9e);
    // Powering the codec off can be done be writing (0x02, 0x9e)

    // Configure codec for fast shutdown.
    audio_write_register(0x0a, 0x00); // Disable the analog soft ramp.
    audio_write_register(0x0e, 0x04); // Disable the digital soft ramp.

    audio_write_register(0x27, 0x00); // Disable the limiter attack level.
    audio_write_register(0x1f, 0x0f); // Adjust bass and treble levels.

    audio_write_register(0x1a, 0x0a); // Adjust PCM volume level.
    audio_write_register(0x1b, 0x0a);
}

void audio_set_volume(int volume) {
    audio_write_register(0x20, (volume + 0x19) & 0xff);
    audio_write_register(0x21, (volume + 0x19) & 0xff);
}

void audio_play_with_callback(AudioCallbackFunction *callback, void *context) {
    audio_stop_dma();

    NVIC_SetPriority(DMA1_Stream7_IRQn, 5);
    NVIC_EnableIRQ(DMA1_Stream7_IRQn);

    SPI3 ->CR2 |= SPI_CR2_TXDMAEN; // Enable I2S TX DMA request.

    callback_function = callback;
    callback_context = context;
    buffer_number = 0;

    if (callback_function)
        callback_function(callback_context, buffer_number);
}

bool audio_provide_buffer_without_blocking(void *samples, int numsamples) {
    if (next_buffer_samples)
        return false;

    NVIC_DisableIRQ(DMA1_Stream7_IRQn);

    next_buffer_samples = samples;
    next_buffer_length = numsamples;

    if (!dma_running)
        audio_start_dma_and_request_buffers();

    NVIC_EnableIRQ(DMA1_Stream7_IRQn);

    return true;
}

void audio_start_dma_and_request_buffers() {
    // Configure DMA stream.
    DMA1_Stream7 ->CR = (0 * DMA_SxCR_CHSEL_0 ) | // Channel 0
        (1 * DMA_SxCR_PL_0 ) | // Priority 1
        (1 * DMA_SxCR_PSIZE_0 ) | // PSIZE = 16 bit
        (1 * DMA_SxCR_MSIZE_0 ) | // MSIZE = 16 bit
        DMA_SxCR_MINC | // Increase memory address
        (1 * DMA_SxCR_DIR_0 ) | // Memory to peripheral
        DMA_SxCR_TCIE; // Transfer complete interrupt
    DMA1_Stream7 ->NDTR = next_buffer_length;
    DMA1_Stream7 ->PAR = (uint32_t) &SPI3 ->DR;
    DMA1_Stream7 ->M0AR = (uint32_t) next_buffer_samples;
    DMA1_Stream7 ->FCR = DMA_SxFCR_DMDIS;
    DMA1_Stream7 ->CR |= DMA_SxCR_EN;

    // Update state.
    next_buffer_samples = (void*)0;
    buffer_number ^= 1;
    dma_running = true;

    // Invoke callback if it exists to queue up another buffer.
    if (callback_function)
        callback_function(callback_context, buffer_number);
}

void audio_stop_dma() {
    DMA1_Stream7 ->CR &= ~DMA_SxCR_EN; // Disable DMA stream.
    while (DMA1_Stream7 ->CR & DMA_SxCR_EN )
        ; // Wait for DMA stream to stop.

    dma_running = false;
}

void DMA1_Stream7_IRQHandler() {
    DMA1 ->HIFCR |= DMA_HIFCR_CTCIF7; // Clear interrupt flag.

    if (next_buffer_samples) {
        audio_start_dma_and_request_buffers();
    } else {
        dma_running = false;
    }
}

// Warning: don't call i2c_write from IRQ handler !
static void audio_write_register(uint8_t address, uint8_t value)
{
    const uint8_t device = 0x4a;
    const uint8_t data[2] = {address, value};
    i2c_transaction_start();
    i2c_write(device, data, 2);
    i2c_transaction_end();
}

