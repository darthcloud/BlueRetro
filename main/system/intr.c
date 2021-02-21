/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include "hal/cpu_ll.h"
#include <xtensa/xtensa_api.h>
#include "intr.h"

#define INT_MUX_DISABLED_INTNO 6

int32_t intexc_alloc_iram(uint32_t source, uint32_t intr_num, XT_INTEXC_HOOK handler) {
    uint32_t intr_level = 0;
    uint32_t core_id = cpu_ll_get_core_id();

    switch (intr_num) {
        case 19:
        case 20:
        case 21:
            intr_level = 2;
            break;
        case 23:
            intr_level = 3;
            break;
        default:
            return -1;
    }

    _xt_intexc_hooks[intr_level] = handler;
    intr_matrix_set(core_id, source, intr_num);
    xt_int_enable_mask(1 << intr_num);

    return 0;
}

int32_t intexc_free_iram(uint32_t source, uint32_t intr_num) {
    uint32_t intr_level = 0;
    uint32_t core_id = cpu_ll_get_core_id();

    switch (intr_num) {
        case 19:
        case 20:
        case 21:
            intr_level = 2;
            break;
        case 23:
            intr_level = 3;
            break;
        default:
            return -1;
    }

    xt_int_disable_mask(1 << intr_num);
    intr_matrix_set(core_id, source, INT_MUX_DISABLED_INTNO);
    _xt_intexc_hooks[intr_level] = 0;

    return 0;
}
