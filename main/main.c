/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "system/bare_metal_app_cpu.h"
#include "system/core0_stall.h"
#include "system/delay.h"
#include "system/fs.h"
#include "system/led.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "wired/detect.h"
#include "wired/npiso_io.h"
#include "wired/cdi_uart.h"
#include "wired/sega_io.h"
#include "wired/nsi.h"
#include "wired/maple.h"
#include "wired/jvs_uart.h"
#include "wired/parallel.h"
#include "wired/pcfx_spi.h"
#include "wired/ps_spi.h"
#include "wired/real_spi.h"
#include "sdkconfig.h"

#ifdef CONFIG_BLUERETRO_SYSTEM_PARALLEL_1P
#define HARDCODED_SYS PARALLEL_1P
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_PARALLEL_2P
#define HARDCODED_SYS PARALLEL_2P
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_NES
#define HARDCODED_SYS NES
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_PCE
#define HARDCODED_SYS PCE
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_GENESIS
#define HARDCODED_SYS GENESIS
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_SNES
#define HARDCODED_SYS SNES
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_CDI
#define HARDCODED_SYS CDI
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_CD32
#define HARDCODED_SYS CD32
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_3DO
#define HARDCODED_SYS REAL_3DO
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_JAGUAR
#define HARDCODED_SYS JAGUAR
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_PSX_PS2
#define HARDCODED_SYS PS2
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_SATURN
#define HARDCODED_SYS SATURN
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_PCFX
#define HARDCODED_SYS PCFX
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_JVS
#define HARDCODED_SYS JVS
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_N64
#define HARDCODED_SYS N64
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_DC
#define HARDCODED_SYS DC
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_GC
#define HARDCODED_SYS GC
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_WII_EXT
#define HARDCODED_SYS WII_EXT
#else
#ifdef CONFIG_BLUERETRO_SYSTEM_EXP_BOARD
#define HARDCODED_SYS EXP_BOARD
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif

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
    "Exp Board",
};

static const wired_init_t wired_init[WIRED_MAX] = {
    NULL, /* WIRED_AUTO */
    parallel_io_init, /* PARALLEL_1P */
    parallel_io_init, /* PARALLEL_2P */
    npiso_init, /* NES */
    NULL, /* PCE */
    sega_io_init, /* GENESIS */
    npiso_init, /* SNES */
    cdi_uart_init, /* CDI */
    NULL, /* CD32 */
    real_spi_init, /* REAL_3DO */
    NULL, /* JAGUAR */
    ps_spi_init, /* PSX */
    sega_io_init, /* SATURN */
    pcfx_spi_init, /* PCFX */
    jvs_init, /* JVS */
    nsi_init, /* N64 */
    maple_init, /* DC */
    ps_spi_init, /* PS2 */
    nsi_init, /* GC */
    NULL, /* WII_EXT */
    NULL, /* EXP_BOARD */
};

static void wired_init_task(void) {
#ifdef CONFIG_BLUERETRO_SYSTEM_UNIVERSAL
    detect_init();
    while (wired_adapter.system_id <= WIRED_AUTO) {
        if (config.magic == CONFIG_MAGIC && config.global_cfg.system_cfg < WIRED_MAX
            && config.global_cfg.system_cfg != WIRED_AUTO) {
            break;
        }
        delay_us(1000);
    }
    detect_deinit();

    if (wired_adapter.system_id >= 0) {
        ets_printf("# Detected system : %d: %s\n", wired_adapter.system_id, sys_name[wired_adapter.system_id]);
    }
#else
    wired_adapter.system_id = HARDCODED_SYS;
    ets_printf("# Hardcoded system : %d: %s\n", wired_adapter.system_id, sys_name[wired_adapter.system_id]);
#endif

    while (config.magic != CONFIG_MAGIC) {
        delay_us(1000);
    }

    if (config.global_cfg.system_cfg < WIRED_MAX && config.global_cfg.system_cfg != WIRED_AUTO) {
        wired_adapter.system_id = config.global_cfg.system_cfg;
        ets_printf("# Config override system : %d: %s\n", wired_adapter.system_id, sys_name[wired_adapter.system_id]);
    }

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        adapter_init_buffer(i);
    }

    if (wired_adapter.system_id < WIRED_MAX && wired_init[wired_adapter.system_id]) {
        wired_init[wired_adapter.system_id]();
    }
}

static void wl_init_task(void *arg) {
    err_led_clear();

    core0_stall_init();

    if (fs_init()) {
        err_led_set();
        printf("FATFS init fail!\n");
    }

    config_init();

#ifndef CONFIG_BLUERETRO_BT_DISABLE
    if (bt_host_init()) {
        err_led_set();
        printf("Bluetooth init fail!\n");
    }
#endif

    vTaskDelete(NULL);
}

void app_main()
{
    adapter_init();

    xTaskCreatePinnedToCore(wl_init_task, "wl_init_task", 4096, NULL, 10, NULL, 0);
    start_app_cpu(wired_init_task);
}

