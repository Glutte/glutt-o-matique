/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Matthias P. Braendli, Maximilien Cuony
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

void batterycharge_init(void);

// Parse a message received from the glutte coulomb counter and update
// internal state accordingly
void batterycharge_push_message(const char *ccounter_msg);

// Get the last received battery capacity in mAh, or 0 if not valid
uint32_t batterycharge_retrieve_last_capacity(void);

// Return 1 if battery charge is too low, -1 if unknown
int batterycharge_too_low(void);

// Return 0 if the wind generator is connected, 1 if the breaker is open, -1 if unknown
int batterycharge_wind_disconnected(void);
