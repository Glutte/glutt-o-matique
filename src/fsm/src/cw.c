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

#include "cw.h"
#include "common.h"

const uint8_t cw_mapping[60] = {
    // Read bits from right to left

    0b110101, //+ ASCII 43
    0b110101, //, ASCII 44
    0b1011110, //- ASCII 45

    0b1010101, //., ASCII 46
    0b110110, // / ASCII 47

    0b100000, // 0, ASCII 48
    0b100001, // 1
    0b100011,
    0b100111,
    0b101111,
    0b111111,
    0b111110,
    0b111100,
    0b111000,
    0b110000, // 9, ASCII 57

    // The following are mostly invalid, but
    // required to fill the gap in ASCII between
    // numerals and capital letters
    0b10, // :
    0b10, // ;
    0b10, // <
    0b10, // =
    0b10, // >
    0b1110011, // ?
    0b1101001, //@

    0b101, // A  ASCII 65
    0b11110,
    0b11010,
    0b1110,
    0b11,
    0b11011,
    0b1100,
    0b11111,
    0b111,
    0b10001,
    0b1010,
    0b11101,
    0b100, //M
    0b110,
    0b1000,
    0b11001,
    0b10100,
    0b1101,
    0b1111,
    0b10,
    0b1011,
    0b10111,
    0b1001,
    0b10110,
    0b10010,
    0b11100, // Z

    0b101010, //Start, ASCII [
    0b1010111, // SK , ASCII '\'
};

void cw_symbol(uint8_t sym)
{
    uint8_t p = 0;
    uint8_t val = cw_mapping[sym];

    while((val >> p) != 0b1) {

        if (((val >> p) & 0b1) == 0b1) {
            dit();
        }
        else {
            dah();
        }

        p++;
    }

    delay_ms(4*DIT_DURATION);
}

// Transmit a string in morse code. Supported range:
// All ASCII between '+' and '\', which includes
// numerals and capital letters.
void tx_cw(char* text)
{
    char* sym = text;
    do {
        cw_symbol(*sym - '+');
        sym++;
    } while (*sym != '\0');
}


