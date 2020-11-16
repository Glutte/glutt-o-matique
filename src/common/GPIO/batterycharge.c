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

#include "Core/common.h"
#include "GPIO/batterycharge.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

// Hysteresis:
#define CHARGE_QRP 1300000
#define CHARGE_QRO 1350000

// If last message older than that, consider the coulomb-counter broken
#define MAX_MESSAGE_AGE 60 // seconds

static uint64_t timestamp_last_message;

static int breaker_valid;
static int breaker_is_open;
static int charge_valid;
static uint32_t last_charge;
static int charge_qrp;

static void reset(void)
{
    breaker_valid = 0;
    breaker_is_open = -1;
    charge_valid = 0;
    last_charge = 0;
    charge_qrp = 0;
}

static void check_timestamp(void)
{
    const uint64_t now = timestamp_now();
    if (timestamp_last_message + MAX_MESSAGE_AGE * 1000 < now) {
        reset();
    }
}

void batterycharge_init()
{
    /* No need to protect the variables, this is called at init before the
     * tasks get created.
     */
    timestamp_last_message = 0;
    reset();
}

void batterycharge_push_message(const char *ccounter_msg)
{
    const uint64_t ts = timestamp_now();

    const int is_capa = strncmp(ccounter_msg, "CAPA", 4) == 0;
    const int is_eol = strncmp(ccounter_msg, "DISJEOL", 7) == 0;

    /* We ignore TEXT, ERROR, VBAT+ and VBAT- for the moment, and only parse CAPA.
     * The \r\n has been trimmed off the message already, therefore the value
     * should extend until the end of string.
     */
    if (is_capa || is_eol) {
        size_t ix_value = 0;
        int second_found = 0;

        for (size_t ix = 0; ccounter_msg[ix] != '\0'; ix++) {
            if (ccounter_msg[ix] == ',') {
                if (! second_found) {
                    second_found = 1;
                }
                else {
                    ix_value = ix + 1;
                }
            }
        }

        const char *valuestr = ccounter_msg + ix_value;
        if (is_capa) {
            /* Field is the capacity in mAh */
            char *endptr;
            errno = 0;
            const uint32_t value = strtol(valuestr, &endptr, 10);
            if (errno == 0 && endptr != valuestr) {
                taskDISABLE_INTERRUPTS();
                last_charge = value;
                charge_valid = 1;
                timestamp_last_message = ts;
                taskENABLE_INTERRUPTS();
            }
        }
        else if (is_eol) {
            /* Field is either `On` or `Off` */
            const int is_on = strncmp(valuestr, "On", 2) == 0;
            const int is_off = strncmp(valuestr, "Off", 2) == 0;

            taskDISABLE_INTERRUPTS();
            if (is_on) {
                breaker_valid = 1;
                breaker_is_open = 0;
                timestamp_last_message = ts;
            }
            else if (is_off) {
                breaker_valid = 1;
                breaker_is_open = 1;
                timestamp_last_message = ts;
            }
            taskENABLE_INTERRUPTS();
        }
    }
}

uint32_t batterycharge_retrieve_last_capacity()
{
    uint32_t ret = 0;
    taskDISABLE_INTERRUPTS();
    check_timestamp();

    if (charge_valid) {
        ret = last_charge;
    }
    taskENABLE_INTERRUPTS();
    return ret;
}

int batterycharge_too_low()
{
    uint32_t c = 0;
    int b = -1;
    taskDISABLE_INTERRUPTS();
    check_timestamp();

    if (charge_valid) {
        c = last_charge;
    }

    if (breaker_valid) {
        b = breaker_is_open;
    }
    taskENABLE_INTERRUPTS();

    /* Disconnected wind generator implies QRP always */
    if (b == 1) {
        return 1;
    }
    else if (c == 0) {
        return -1;
    }
    /* Implement hysteresis */
    else if (charge_qrp == 0 && c < CHARGE_QRP) {
        charge_qrp = 1;
    }
    else if (charge_qrp == 1 && c >= CHARGE_QRO) {
        charge_qrp = 0;
    }
    return charge_qrp;
}

int batterycharge_wind_disconnected()
{
    int b = -1;
    taskDISABLE_INTERRUPTS();
    check_timestamp();

    if (breaker_valid) {
        b = breaker_is_open;
    }
    taskENABLE_INTERRUPTS();

    return b;
}
