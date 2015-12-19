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

#include <stdint.h>
#include <string.h>
#include "ubx.h"

static ubx_callbacks_t        ubx_cb;

void ubx_register(const ubx_callbacks_t *cb)
{
    memcpy(&ubx_cb, cb, sizeof(ubx_callbacks_t));
}

static ubx_nav_sol_t     navsol_temp;
static ubx_nav_timeutc_t navtimeutc_temp;
static ubx_tim_tm2_t     timtm2_temp;

// State machine variables
static ubx_state_t ubxState = UBXSTATE_SYNC1;
static uint16_t msglen;
static uint8_t* messagedata;
static uint8_t msgclass;
static uint8_t msgid;
static uint8_t cka, ckb;

int ubx_parse(const uint8_t c)
{
    switch (ubxState)
    {
        case UBXSTATE_SYNC1:
            if (c == UBX_SYNC1_CHAR) {
                ubxState = UBXSTATE_SYNC2;
            }
            break;

        case UBXSTATE_SYNC2:
            if (c == UBX_SYNC2_CHAR) {
                ubxState = UBXSTATE_CLASS;
            }
            else if (c == UBX_SYNC1_CHAR) {
                ubxState = UBXSTATE_SYNC2;
            }
            else {
                ubxState = UBXSTATE_SYNC1;
            }
            break;

        case UBXSTATE_CLASS:
            ubxState = UBXSTATE_ID;
            msgclass = c;

            // re-init checksum
            cka = c;
            ckb = cka;
            break;

        case UBXSTATE_ID:
            msgid = c;
            ubxState = UBXSTATE_LEN1;
            if (msgclass == UBX_CLASS_NAV) {
                if (c == UBX_NAV_SOL) {
                    messagedata = (uint8_t*)&navsol_temp;
                }
                else if (c == UBX_NAV_TIMEUTC) {
                    messagedata = (uint8_t*)&navtimeutc_temp;
                }
            }
            else if (msgclass == UBX_CLASS_TIM && c == UBX_TIM_TM2) {
                messagedata = (uint8_t*)&timtm2_temp;
            }
            else {
                msgclass = 0;
                msgid = 0;
                ubxState = UBXSTATE_SYNC1;
            }
            cka += c;
            ckb += cka;
            break;

        case UBXSTATE_LEN1:
            msglen   = (uint16_t)c;
            ubxState = UBXSTATE_LEN2;
            cka += c;
            ckb += cka;
            break;

        case UBXSTATE_LEN2:
            msglen  |= ((uint16_t)c) << 8;
            ubxState = UBXSTATE_DATA;
            cka += c;
            ckb += cka;
            break;

        case UBXSTATE_DATA:
            cka += c;
            ckb += cka;
            if (--msglen) {
                *(messagedata++) = c;
            }
            else {
                ubxState = UBXSTATE_CKA;
            }
            break;

        case UBXSTATE_CKA:
            if (c == cka) {
                ubxState = UBXSTATE_CKB;
            }
            else {
                ubxState = UBXSTATE_SYNC1;
            }
            break;

        case UBXSTATE_CKB:
            if (c == ckb) {
                if (msgclass == UBX_CLASS_NAV) {
                    if (msgid == UBX_NAV_SOL && ubx_cb.new_ubx_nav_sol) {
                        ubx_cb.new_ubx_nav_sol(&navsol_temp);
                    }
                    else if (msgid == UBX_NAV_TIMEUTC && ubx_cb.new_ubx_nav_timeutc) {
                        ubx_cb.new_ubx_nav_timeutc(&navtimeutc_temp);
                    }
                }
                else if (msgclass == UBX_CLASS_TIM) {
                    if (msgid == UBX_TIM_TM2 && ubx_cb.new_ubx_tim_tm2) {
                        ubx_cb.new_ubx_tim_tm2(&timtm2_temp);
                    }
                }
            }

            ubxState = UBXSTATE_SYNC1;
    }
    return ubxState;
}

