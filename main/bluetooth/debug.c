/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <xtensa/hal.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#include "tools/stats.h"

static uint8_t type;
static uint32_t counter;
static uint32_t start, end;

void bt_dbg_init(uint8_t dev_type) {
    end = 0;
    start = xthal_get_ccount();
    counter = 0;
    type = dev_type;
}

void bt_dbg(uint8_t *data, uint16_t len) {
#ifdef CONFIG_BLUERETRO_BT_REPORT_INTERVAL_STATS
        float average, max, min, std_dev;
        uint32_t interval;

        end = xthal_get_ccount();
        counter++;

        if (end > start) {
            interval = (end - start)/CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ;
        }
        else {
            interval = ((0xFFFFFFFF - start) + end)/CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ;
        }
        start = end;

        average = getAverage(interval)/1000;
        max = getMax(interval)/1000;
        min = getMin(interval)/1000;
        std_dev = getStdDev(interval)/1000;
        printf("\e[1;1H\e[2J");

        printf("Samples: %ld\n", counter);
        printf("Average: %.6f ms\n", average);
        printf("Max:     %.6f ms\n", max);
        printf("Min:     %.6f ms\n", min);
        printf("Std Dev: %.6f ms\n", std_dev);
#else
#ifdef CONFIG_BLUERETRO_BT_MIN_LATENCY_TEST_PS3
        if ((*(uint32_t *)&data[11]) & 0x01FFFF00) {
            GPIO.out = 0xFBFFFFFF;
        }
        else {
            GPIO.out = 0xFFFFFFFF;
        }
#else
#ifdef CONFIG_BLUERETRO_BT_MIN_LATENCY_TEST_PS4
        if ((*(uint32_t *)&data[6+11]) & 0x0003FFF0) {
            GPIO.out = 0xFBFFFFFF;
        }
        else {
            GPIO.out = 0xFFFFFFFF;
        }
#else
#ifdef CONFIG_BLUERETRO_BT_MIN_LATENCY_TEST_PS5
        if ((*(uint32_t *)&data[8+11]) & 0x0003FFF0) {
            GPIO.out = 0xFBFFFFFF;
        }
        else {
            GPIO.out = 0xFFFFFFFF;
        }
#else
#ifdef CONFIG_BLUERETRO_BT_MIN_LATENCY_TEST_WIIU
        if (~(*(uint32_t *)&data[13+11]) & 0x0003FFFE) {
            GPIO.out = 0xFBFFFFFF;
        }
        else {
            GPIO.out = 0xFFFFFFFF;
        }
#else
#ifdef CONFIG_BLUERETRO_BT_MIN_LATENCY_TEST_XB1
        if ((*(uint32_t *)&data[13+11]) & 0x000003FF) {
            GPIO.out = 0xFBFFFFFF;
        }
        else {
            GPIO.out = 0xFFFFFFFF;
        }
#else
#ifdef CONFIG_BLUERETRO_BT_MIN_LATENCY_TEST_SW
        if ((*(uint16_t *)&data[+11])) {
            GPIO.out = 0xFBFFFFFF;
        }
        else {
            GPIO.out = 0xFFFFFFFF;
        }
#endif
#endif
#endif
#endif
#endif
#endif
#endif
}

