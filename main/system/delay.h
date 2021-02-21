/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _DELAY_H_
#define _DELAY_H_

#include "xtensa/core-macros.h"

inline void delay_us(uint32_t delay_us) {
    uint32_t start, cur, timeout;
    start = xthal_get_ccount();
    timeout = CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ * delay_us;
    do {
        cur = xthal_get_ccount();
    } while (cur - start < timeout);
}

#endif /* _DELAY_H_ */
