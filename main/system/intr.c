/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <esp32/rom/ets_sys.h>
#include <esp_cpu.h>
#include "intr.h"

#define INT_MUX_DISABLED_INTNO 6

/*
-------------------------------------------------------------------------------
  Call this function to disable non iram located interrupts.
    newmask       - mask containing the interrupts to disable.
-------------------------------------------------------------------------------
*/
static inline uint32_t xt_int_disable_mask(uint32_t newmask)
{
    uint32_t oldint;
    asm volatile (
        "movi %0,0\n"
        "xsr %0,INTENABLE\n"    //disable all ints first
        "rsync\n"
        "and a3,%0,%1\n"        //mask ints that need disabling
        "wsr a3,INTENABLE\n"    //write back
        "rsync\n"
        :"=&r"(oldint):"r"(newmask):"a3");

    return oldint;
}

/*
-------------------------------------------------------------------------------
  Call this function to enable non iram located interrupts.
    newmask       - mask containing the interrupts to enable.
-------------------------------------------------------------------------------
*/
static inline void xt_int_enable_mask(uint32_t newmask)
{
    asm volatile (
        "movi a3,0\n"
        "xsr a3,INTENABLE\n"
        "rsync\n"
        "or a3,a3,%0\n"
        "wsr a3,INTENABLE\n"
        "rsync\n"
        ::"r"(newmask):"a3");
}

int32_t intexc_alloc_iram(uint32_t source, uint32_t intr_num, XT_INTEXC_HOOK handler) {
    uint32_t intr_level = 0;
    uint32_t core_id = esp_cpu_get_core_id();

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
    uint32_t core_id = esp_cpu_get_core_id();

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
