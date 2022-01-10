/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include "npiso_io.h"
#include "cdi_uart.h"
#include "sega_io.h"
#include "pce_io.h"
#include "nsi.h"
#include "maple.h"
#include "jvs_uart.h"
#include "parallel.h"
#include "pcfx_spi.h"
#include "ps_spi.h"
#include "real_spi.h"
#include "jag_io.h"
#include "adapter/adapter.h"

typedef void (*wired_init_t)(void);

static const char *sys_name[WIRED_MAX] = {
    "AUTO",
    "PARALLEL_1P",
    "PARALLEL_2P",
    "NES",
    "PCE",
    "MD-GENESIS",
    "SNES",
    "CD-i",
    "CD32",
    "3DO",
    "JAGUAR",
    "PSX",
    "SATURN",
    "PC-FX",
    "JVS",
    "N64",
    "DC",
    "PS2",
    "GC",
    "Wii-EXT",
    "VB",
    "Exp Board",
};

static const wired_init_t wired_init[WIRED_MAX] = {
    NULL, /* WIRED_AUTO */
    parallel_io_init, /* PARALLEL_1P */
    parallel_io_init, /* PARALLEL_2P */
    npiso_init, /* NES */
    pce_io_init, /* PCE */
    sega_io_init, /* GENESIS */
    npiso_init, /* SNES */
    cdi_uart_init, /* CDI */
    NULL, /* CD32 */
    real_spi_init, /* REAL_3DO */
    jag_io_init, /* JAGUAR */
    ps_spi_init, /* PSX */
    sega_io_init, /* SATURN */
    pcfx_spi_init, /* PCFX */
    jvs_init, /* JVS */
    nsi_init, /* N64 */
    maple_init, /* DC */
    ps_spi_init, /* PS2 */
    nsi_init, /* GC */
    NULL, /* WII_EXT */
    npiso_init, /* VB */
    NULL, /* EXP_BOARD */
};

void wired_comm_init(void) {
    if (wired_init[wired_adapter.system_id]) {
        wired_init[wired_adapter.system_id]();
    }
}

const char *wired_get_sys_name(void) {
    return sys_name[wired_adapter.system_id];
}
