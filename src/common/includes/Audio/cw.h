/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Matthias P. Braendli
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

#ifndef __CW_H_
#define __CW_H_

#include <stdint.h>
#include <stddef.h>

// Setup the CW generator to create audio samples at the given
// samplerate.
void cw_psk31_init(unsigned int samplerate);

// Append new CW or PSK31 text to transmit
// CW/PSK31 audio centre frequency in Hz
// if dit_duration == 0, message is sent in PSK31
// otherwise it is sent in CW, with dit_duration in ms
// returns 0 on failure, 1 on success
int cw_psk31_push_message(const char* text, int frequency, int dit_duration);

// Write the waveform into the buffer (stereo), both for cw and psk31
size_t cw_psk31_fill_buffer(int16_t *buf, size_t bufsize);

// Return 1 if the CW or PSK31 generator is running
int cw_psk31_busy(void);

void cw_message_sent(const char*);

#endif // __CW_H_

