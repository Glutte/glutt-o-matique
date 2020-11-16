/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Matthias P. Braendli
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

#pragma once

#include <stdint.h>
#include <stddef.h>

// Setup the CW generator to create audio samples at the given
// samplerate.
void cw_psk_init(unsigned int samplerate);

// Append new CW or PSK text to transmit
// CW/PSK audio centre frequency in Hz
//
// Supported characters for CW:
// All ASCII between '+' and '\', which includes
// numerals and capital letters.
//
// Supported characters for PSK: 7-bit clean ASCII
//
// if dit_duration == -1, message is sent in PSK31
// if dit_duration == -2, message is sent in PSK63
// if dit_duration == -3, message is sent in PSK125
// otherwise it is sent in CW, with dit_duration in ms
// returns 0 on failure, 1 on success
int cw_psk_push_message(const char* text, int frequency, int dit_duration);

// Write the waveform into the buffer (stereo), both for cw and psk
size_t cw_psk_fill_buffer(int16_t *buf, size_t bufsize);

// Return 1 if the CW or PSK generator is running
int cw_psk_busy(void);

void cw_message_sent(const char*);

size_t cw_symbol(uint8_t, uint8_t *, size_t);

