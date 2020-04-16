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

#define BMP085_ADDRESS (0x77)

typedef enum {
      BMP085_REGISTER_CAL_AC1          = 0xAA,
      BMP085_REGISTER_CAL_AC2          = 0xAC,
      BMP085_REGISTER_CAL_AC3          = 0xAE,
      BMP085_REGISTER_CAL_AC4          = 0xB0,
      BMP085_REGISTER_CAL_AC5          = 0xB2,
      BMP085_REGISTER_CAL_AC6          = 0xB4,
      BMP085_REGISTER_CAL_B1           = 0xB6,
      BMP085_REGISTER_CAL_B2           = 0xB8,
      BMP085_REGISTER_CAL_MB           = 0xBA,
      BMP085_REGISTER_CAL_MC           = 0xBC,
      BMP085_REGISTER_CAL_MD           = 0xBE,
      BMP085_REGISTER_CHIPID           = 0xD0,
      BMP085_REGISTER_VERSION          = 0xD1,
      BMP085_REGISTER_SOFTRESET        = 0xE0,
      BMP085_REGISTER_CONTROL          = 0xF4,
      BMP085_REGISTER_TEMPDATA         = 0xF6,
      BMP085_REGISTER_PRESSUREDATA     = 0xF6,
} bmp085_register_address_t;

typedef enum {
      BMP085_COMMAND_READTEMP      = 0x2E,
      BMP085_COMMAND_READPRESSURE  = 0x34
} bmp085_command_t;

typedef enum {
  BMP085_MODE_ULTRALOWPOWER          = 0,
  BMP085_MODE_STANDARD               = 1,
  BMP085_MODE_HIGHRES                = 2,
  BMP085_MODE_ULTRAHIGHRES           = 3
} bmp085_mode_t;


typedef struct {
    int16_t  ac1;
    int16_t  ac2;
    int16_t  ac3;
    uint16_t ac4;
    uint16_t ac5;
    uint16_t ac6;
    int16_t  b1;
    int16_t  b2;
    int16_t  mb;
    int16_t  mc;
    int16_t  md;
} bmp085_calib_data_t;

// Returns 1 on success
int bmp085_init();

// Returns 1 on success
int bmp085_get_temp_pressure(int32_t *temperature_deciDeg, int32_t *pressure_Pa);
