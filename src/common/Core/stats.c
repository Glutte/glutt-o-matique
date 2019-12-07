/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Matthias P. Braendli, Maximilien Cuony
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

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "Core/stats.h"
#include "Core/common.h"
#include "vc.h"

static int values_valid = 0;
static int num_beacons_sent = 0;
static int num_wind_generator_movements = 0;
static int num_tx_switch = 0;
static int num_antibavard = 0;
static int num_sv_used = 0;
static int num_qro = 0;
static int num_qrp = 0;
static float battery_min = -1.0f;
static float battery_max = -1.0f;
static float battery_per_hour[24];
#define TEMP_INVALID -128.0f
static float temp_min = TEMP_INVALID;
static float temp_max = TEMP_INVALID;

static int last_tx_on_valid = 0;
static uint64_t last_tx_on = 0;
static uint64_t max_qso_duration = 0;

/* Ideas
 *
 * Version
 * Uptime
 * Number of beacons
 * Ubat min/max/avg
 * Temperature min/max/avg
 * QRP/QRO time ratio in %
 * Number of K, G, D, U, S, R sent
 * Number of TX On/Off
 * How many times anti-bavard got triggered
 * Max SWR ratio
 * Number of wind generator movements
 * Longest QSO duration
 * Number of GNSS SVs tracked
 */

#define STATS_LEN 1024 // also check MAX_MESSAGE_LEN in cw.c
static char stats_text[STATS_LEN];
static int32_t stats_end_ix = 0;

static void clear_stats()
{
    num_beacons_sent = 0;
    num_wind_generator_movements = 0;
    num_tx_switch = 0;
    num_antibavard = 0;
    battery_min = -1.0f;
    battery_max = -1.0f;
    temp_min = TEMP_INVALID;
    temp_max = TEMP_INVALID;
    num_qro = 0;
    num_qrp = 0;
    last_tx_on_valid = 0;
    max_qso_duration = 0;
    for (int i = 0; i < 24; i++) {
        battery_per_hour[i] = -1.0f;
    }
    values_valid = 1;
}

void stats_voltage_at_full_hour(int hour, float u_bat)
{
    if (values_valid == 0) {
        clear_stats();
    }

    if (hour > 0 && hour < 24) {
        battery_per_hour[hour] = u_bat;
    }
}

void stats_voltage(float u_bat)
{
    if (values_valid == 0) {
        clear_stats();
    }

    if (u_bat < battery_min || battery_min == -1.0f) {
        battery_min = u_bat;
    }

    if (u_bat > battery_max || battery_max == -1.0f) {
        battery_max = u_bat;
    }
}

void stats_temp(float temp)
{
    if (values_valid == 0) {
        clear_stats();
    }

    if (temp < temp_min || temp_min == TEMP_INVALID) {
        temp_min = temp;
    }

    if (temp > temp_max || temp_max == TEMP_INVALID) {
        temp_max = temp;
    }
}

void stats_qrp(int is_qrp)
{
    if (values_valid == 0) {
        clear_stats();
    }

    if (is_qrp) {
        num_qrp++;
    }
    else {
        num_qro++;
    }
}

void stats_wind_generator_moved()
{
    if (values_valid == 0) {
        clear_stats();
    }

    num_wind_generator_movements++;
}

void stats_beacon_sent()
{
    if (values_valid == 0) {
        clear_stats();
    }

    num_beacons_sent++;
}

void stats_tx_switched(int tx_on)
{
    if (values_valid == 0) {
        clear_stats();
    }

    num_tx_switch++;

    if (tx_on) {
        last_tx_on = timestamp_now();
        last_tx_on_valid = 1;
    }
    else if (last_tx_on_valid) {
        const uint64_t qso_duration = timestamp_now() - last_tx_on;
        if (qso_duration > max_qso_duration) {
            max_qso_duration = qso_duration;
        }
    }
}

void stats_anti_bavard_triggered()
{
    if (values_valid == 0) {
        clear_stats();
    }

    num_antibavard++;
}

void stats_num_gnss_sv(int num_sv)
{
    if (values_valid == 0) {
        clear_stats();
    }

    num_sv_used = num_sv;
}

const char* stats_build_text(void)
{
    struct tm time = {0};
    int time_valid = local_time(&time);

    stats_end_ix = snprintf(stats_text + stats_end_ix, STATS_LEN - 1 - stats_end_ix,
            "HB9G www.glutte.ch HB9G www.glutte.ch\n");

    if (time_valid) {
        stats_end_ix += snprintf(stats_text + stats_end_ix, STATS_LEN - 1 - stats_end_ix,
                "Statistiques du %04d-%02d-%02d\n",
                time.tm_year + 1900, time.tm_mon + 1, time.tm_mday);
    }
    else {
        stats_end_ix += snprintf(stats_text + stats_end_ix, STATS_LEN - 1 - stats_end_ix,
                "Statistiques de la journee\n");
    }

    uint64_t uptime = timestamp_now();
    uint64_t uptime_j = uptime / (24 * 3600 * 1000);
    uptime -= uptime_j * (24 * 3600 * 1000);
    uint64_t uptime_h = uptime / (3600 * 1000);
    uptime -= uptime_h * (3600 * 1000);
    uint64_t uptime_m = uptime / (60 * 1000);

    stats_end_ix += snprintf(stats_text + stats_end_ix, STATS_LEN - 1 - stats_end_ix,
            "Version= %s\n"
            "Uptime= %dj%dh%dm\n",
            vc_get_version(),
            (int)uptime_j, (int)uptime_h, (int)uptime_m);

    if (values_valid == 0) {
        return stats_text;
    }

    const int battery_min_decivolt = 10.0f * battery_min;
    const int battery_max_decivolt = 10.0f * battery_max;

    stats_end_ix += snprintf(stats_text + stats_end_ix, STATS_LEN - 1 - stats_end_ix,
            "U min,max= %dV%01d,%dV%01d\n"
            "Temps QRP= %d%%\n",
            battery_min_decivolt / 10, battery_min_decivolt % 10,
            battery_max_decivolt / 10, battery_max_decivolt % 10,
            100 * num_qrp / (num_qrp + num_qro));

    stats_end_ix += snprintf(stats_text + stats_end_ix, STATS_LEN - 1 - stats_end_ix,
            "U heures pleines= ");

    for (int h = 0; h < 24; h++) {
        if (battery_per_hour[h] == -1.0f) {
            stats_end_ix += snprintf(stats_text + stats_end_ix, STATS_LEN - 1 - stats_end_ix,
                    " ?");
        }
        else {
            const int battery_decivolts = 10.0f * battery_per_hour[h];
            stats_end_ix += snprintf(stats_text + stats_end_ix, STATS_LEN - 1 - stats_end_ix,
                    " %dV%01d",
                    battery_decivolts / 10, battery_decivolts % 10);
        }
    }

    stats_end_ix += snprintf(stats_text + stats_end_ix, STATS_LEN - 1 - stats_end_ix,
            "\n"
            "Nbre de commutations eolienne= %d\n",
            num_wind_generator_movements);

    if (temp_min != TEMP_INVALID && temp_max != TEMP_INVALID) {
        const int temp_min_positive = (temp_min > 0);
        const int temp_max_positive = (temp_max > 0);

        const int temp_min_decidegree = 10.0f * fabsf(temp_min);
        const int temp_max_decidegree = 10.0f * fabsf(temp_max);
        stats_end_ix += snprintf(stats_text + stats_end_ix, STATS_LEN - 1 - stats_end_ix,
                "Temp min,max= %s%dC%01d,%s%dC%01d\n",
                temp_min_positive ? "" : "-",
                temp_min_decidegree / 10, temp_min_decidegree % 10,
                temp_max_positive ? "" : "-",
                temp_max_decidegree / 10, temp_max_decidegree % 10);
    }

    uint64_t qso_duration = max_qso_duration;
    int qso_duration_h = qso_duration / (3600 * 1000);
    qso_duration -= qso_duration_h * ( 3600 * 1000);
    int qso_duration_m = qso_duration / (60 * 1000);
    qso_duration -= qso_duration_m * (60 * 1000);
    int qso_duration_s = qso_duration / (1000);

    stats_end_ix += snprintf(stats_text + stats_end_ix, STATS_LEN - 1 - stats_end_ix,
            "Nbre de balises= %d\n"
            "Nbre de TX ON/OFF= %d\n"
            "Nbre anti-bavard= %d\n"
            "QSO le plus long= %dh%dm%ds\n"
            "Sat GPS= %d\n",
            num_beacons_sent,
            num_tx_switch,
            num_antibavard,
            qso_duration_h, qso_duration_m, qso_duration_s,
            num_sv_used
            );

    values_valid = 0;

    return stats_text;
}
