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

#include "GPIO/analog.h"

#define SUPPLY_HISTORY_LEN 10

static float supply_history[SUPPLY_HISTORY_LEN];
static int supply_history_ix = 0;
static int supply_history_ready = 0;
static int last_qrp = 0;

// Return 1 if analog supply is too low
int analog_supply_too_low(void)
{

    // Hysteresis:
    // Lower voltage 12.5V
    // High  voltage 13V
    const float measure = analog_measure_12v();

    supply_history[supply_history_ix] = measure;
    supply_history_ix++;
    if (supply_history_ix >= SUPPLY_HISTORY_LEN) {
        supply_history_ix = 0;
        supply_history_ready = 1;
    }

    if (supply_history_ready) {
        float sum = 0.0f;
        for (int i = 0; i < SUPPLY_HISTORY_LEN; i++) {
            sum += supply_history[i];
        }
        int above_lower_limit = (sum > 12.5f * SUPPLY_HISTORY_LEN);
        int above_upper_limit = (sum > 13.0f * SUPPLY_HISTORY_LEN);

        if (!above_lower_limit) {
            last_qrp = 1;
        }
        else if (above_upper_limit) {
            last_qrp = 0;
        }

        return last_qrp;
    }

    return 0;
}
