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
#include "sea_io.h"
#include "wii_i2c.h"
#include "adapter/adapter.h"

typedef void (*wired_init_t)(void);
typedef void (*wired_port_cfg_t)(uint16_t mask);

static const char *sys_name[WIRED_MAX] = {
    "AUTO",
    "PARALLEL_1P_PP",
    "PARALLEL_2P_PP",
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
    "PARALLEL_1P_OD",
    "PARALLEL_2P_OD",
    "SEA Board",
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
    wii_i2c_init, /* WII_EXT */
    npiso_init, /* VB */
    parallel_io_init, /* PARALLEL_1P_OD */
    parallel_io_init, /* PARALLEL_2P_OD */
    sea_init, /* SEA_BOARD */
};

static const wired_port_cfg_t wired_port_cfg[WIRED_MAX] = {
    NULL, /* WIRED_AUTO */
    NULL, /* PARALLEL_1P */
    NULL, /* PARALLEL_2P */
    NULL, /* NES */
    NULL, /* PCE */
    NULL, /* GENESIS */
    NULL, /* SNES */
    NULL, /* CDI */
    NULL, /* CD32 */
    NULL, /* REAL_3DO */
    NULL, /* JAGUAR */
    NULL, /* PSX */
    NULL, /* SATURN */
    NULL, /* PCFX */
    NULL, /* JVS */
    NULL, /* N64 */
    NULL, /* DC */
    NULL, /* PS2 */
    NULL, /* GC */
    wii_i2c_port_cfg, /* WII_EXT */
    NULL, /* VB */
    NULL, /* PARALLEL_1P_OD */
    NULL, /* PARALLEL_2P_OD */
    NULL, /* SEA_BOARD */
};

void wired_comm_init(void) {
    if (wired_init[wired_adapter.system_id]) {
        wired_init[wired_adapter.system_id]();
    }
}

void wired_comm_port_cfg(uint16_t mask) {
    if (wired_port_cfg[wired_adapter.system_id]) {
        wired_port_cfg[wired_adapter.system_id](mask);
    }
}

const char *wired_get_sys_name(void) {
    return sys_name[wired_adapter.system_id];
}
