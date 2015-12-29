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

/* An implementation of a BPSK31 generator.
 */

#ifndef __PSK_31_H_
#define __PSK_31_H_
#include <stdint.h>
#include <stddef.h>

void psk31_init(unsigned int samplerate);

// Append new psk31 text to transmit
// PSK31 audio centre frequency in Hz
// returns 0 on failure, 1 on success
int psk31_push_message(const char* text, int frequency);


// Write the waveform into the buffer (stereo)
size_t psk31_fill_buffer(int16_t *buf, size_t bufsize);

// Return 1 if the psk31 generator has completed transmission
int psk31_busy(void);

#endif // __PSK_31_H_

