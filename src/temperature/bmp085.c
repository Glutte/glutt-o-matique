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

#include "FreeRTOS.h"
#include "task.h"
#include "bmp085.h"
#include "i2c.h"
#include "debug.h"
#include <stdio.h>

const uint8_t mode = BMP085_MODE_ULTRAHIGHRES;

static int init_ok = 0;
static bmp085_calib_data_t bmp085_coeffs;

static int bmp085_raw_temp(int32_t *raw_temp);
static int bmp085_raw_pressure(int32_t *raw_pressure);
static int read_u16(uint8_t reg, uint16_t *value);
static int read_s16(uint8_t reg, int16_t *value);

int read_u16(uint8_t reg, uint16_t *value)
{
    int success = 0;
    if (i2c_transaction_start()) {
        success = i2c_read_from(BMP085_ADDRESS, reg, (uint8_t*)value, 2) > 0;
        i2c_transaction_end();
    }
    else {
        debug_print("read_u16 no transaction!\n");
    }
    return success;
}

int read_s16(uint8_t reg, int16_t *value) {
    uint16_t v;
    int success = read_u16(reg, &v);
    *value = (int16_t)v;
    return success;
}

static void print_s16(const char *hdr, int16_t value) {
    char t[32];
    snprintf(t, 32, "%s=%d\n", hdr, value);
    debug_print(t);
}

static void print_s32(const char *hdr, int32_t value) {
    char t[32];
    snprintf(t, 32, "%s=%ld\n", hdr, value);
    debug_print(t);
}

static void print_u16(const char *hdr, uint16_t value) {
    char t[32];
    snprintf(t, 32, "%s=%d\n", hdr, value);
    debug_print(t);
}

int bmp085_init() {
    uint8_t id = 0;

    int success = 0;
    if (i2c_transaction_start()) {
        success = i2c_read_from(BMP085_ADDRESS, BMP085_REGISTER_CHIPID, &id, 1) > 0;
        i2c_transaction_end();
    }
    else {
        debug_print("bmp085_init no transaction");
    }

    if (success == 0 || id != 0x55) {
        return 0;
    }

    success &= read_s16(BMP085_REGISTER_CAL_AC1, &bmp085_coeffs.ac1);
    success &= read_s16(BMP085_REGISTER_CAL_AC2, &bmp085_coeffs.ac2);
    success &= read_s16(BMP085_REGISTER_CAL_AC3, &bmp085_coeffs.ac3);
    success &= read_u16(BMP085_REGISTER_CAL_AC4, &bmp085_coeffs.ac4);
    success &= read_u16(BMP085_REGISTER_CAL_AC5, &bmp085_coeffs.ac5);
    success &= read_u16(BMP085_REGISTER_CAL_AC6, &bmp085_coeffs.ac6);
    success &= read_s16(BMP085_REGISTER_CAL_B1, &bmp085_coeffs.b1);
    success &= read_s16(BMP085_REGISTER_CAL_B2, &bmp085_coeffs.b2);
    success &= read_s16(BMP085_REGISTER_CAL_MB, &bmp085_coeffs.mb);
    success &= read_s16(BMP085_REGISTER_CAL_MC, &bmp085_coeffs.mc);
    success &= read_s16(BMP085_REGISTER_CAL_MD, &bmp085_coeffs.md);

    print_s16("AC1", bmp085_coeffs.ac1);
    print_s16("AC2", bmp085_coeffs.ac2);
    print_s16("AC3", bmp085_coeffs.ac3);
    print_u16("AC4", bmp085_coeffs.ac4);
    print_u16("AC5", bmp085_coeffs.ac5);
    print_u16("AC6", bmp085_coeffs.ac6);
    print_s16("B1", bmp085_coeffs.b1);
    print_s16("B2", bmp085_coeffs.b2);
    print_s16("MB", bmp085_coeffs.mb);
    print_s16("MC", bmp085_coeffs.mc);
    print_s16("MD", bmp085_coeffs.md);

    init_ok = success;
    return success;
}


int bmp085_raw_temp(int32_t *raw_temp)
{
    int success = 1;
    const uint8_t data[2] = {BMP085_REGISTER_CONTROL, BMP085_COMMAND_READTEMP};
    if (i2c_transaction_start()) {
        success &= i2c_write(BMP085_ADDRESS, data, 2);
        i2c_transaction_end();
    }
    else {
        return 0;
    }

    vTaskDelay(15 / portTICK_PERIOD_MS);

    uint16_t t;
    success &= read_u16(BMP085_REGISTER_TEMPDATA, &t);
    *raw_temp = t;
    return success;
}

int bmp085_raw_pressure(int32_t *raw_pressure)
{
    int success = 1;

    const uint8_t data[2] = {BMP085_REGISTER_CONTROL, BMP085_COMMAND_READPRESSURE | (mode << 6)};
    if (i2c_transaction_start()) {
        success &= i2c_write(BMP085_ADDRESS, data, 2);
        i2c_transaction_end();
    }
    else {
        return 0;
    }

    int delay = 0;
    switch (mode) {
        case BMP085_MODE_ULTRALOWPOWER:
            delay = 5;
            break;
        case BMP085_MODE_STANDARD:
            delay = 8;
            break;
        case BMP085_MODE_HIGHRES:
            delay = 14;
            break;
        case BMP085_MODE_ULTRAHIGHRES:
        default:
            delay = 26;
            break;
    }
    vTaskDelay(3*delay / portTICK_PERIOD_MS);

    uint8_t  p8;
    uint16_t p16;
    int32_t  p32;

    success &= read_u16(BMP085_REGISTER_PRESSUREDATA, &p16);
    p32 = (uint32_t)p16 << 8;

    if (i2c_transaction_start()) {
        success &= i2c_read_from(BMP085_ADDRESS, BMP085_REGISTER_PRESSUREDATA+2, &p8, 1) > 0;
        i2c_transaction_end();
    }
    p32 += p8;
    p32 >>= (8 - mode);
    *raw_pressure = p32;
    return success;
}

int bmp085_get_temp_pressure(int32_t *temperature_deciDeg, int32_t *pressure_Pa)
{
    if (!init_ok) {
        return 0;
    }

    int32_t  compp = 0;
    int32_t  x1, x2, b5, b6, x3, b3, p;
    uint32_t b4, b7;

    int32_t ut = 0;
    int success = bmp085_raw_temp(&ut);
    print_s32("ut", ut);
    int32_t up = 0;
    success &= bmp085_raw_pressure(&up);
    print_s32("up", up);

    /* Temperature compensation */
    int32_t X1 = (ut - (int32_t)bmp085_coeffs.ac6) * ((int32_t)bmp085_coeffs.ac5) >> 15;
    int32_t X2 = ((int32_t)bmp085_coeffs.mc << 11) / (X1+(int32_t)bmp085_coeffs.md);
    b5 = X1 + X2;
    int32_t temp = (b5 + 8) >> 4;
    *temperature_deciDeg = temp;

    /* Pressure compensation */
    b6 = b5 - 4000;
    x1 = (bmp085_coeffs.b2 * ((b6 * b6) >> 12)) >> 11;
    x2 = (bmp085_coeffs.ac2 * b6) >> 11;
    x3 = x1 + x2;
    b3 = (((((int32_t) bmp085_coeffs.ac1) * 4 + x3) << mode) + 2) >> 2;
    x1 = (bmp085_coeffs.ac3 * b6) >> 13;
    x2 = (bmp085_coeffs.b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    b4 = (bmp085_coeffs.ac4 * (uint32_t) (x3 + 32768)) >> 15;
    b7 = ((uint32_t) (up - b3) * (50000 >> mode));

    if (b7 < 0x80000000) {
        p = (b7 << 1) / b4;
    }
    else {
        p = (b7 / b4) << 1;
    }

    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    compp = p + ((x1 + x2 + 3791) >> 4);

    /* Assign compensated pressure value */
    *pressure_Pa = compp;

    return success;
}
