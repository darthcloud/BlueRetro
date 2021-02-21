/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "drivers/sd.h"
#include "drivers/led.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "wired/detect.h"
#include "wired/npiso.h"
#include "wired/sega_io.h"
#include "wired/nsi.h"
#include "wired/maple.h"
#include "wired/jvs.h"
#include "wired/parallel.h"
#include "wired/ps_spi.h"

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
    NULL, /* CDI */
    NULL, /* CD32 */
    NULL, /* REAL_3DO */
    NULL, /* JAGUAR */
    ps_spi_init, /* PSX */
    sega_io_init, /* SATURN */
    NULL, /* PCFX */
    jvs_init, /* JVS */
    nsi_init, /* N64 */
    maple_init, /* DC */
    ps_spi_init, /* PS2 */
    nsi_init, /* GC */
    NULL, /* WII_EXT */
    NULL, /* EXP_BOARD */
};

static void wired_init_task(void *arg) {
    adapter_init();

#if 1
    detect_init();
    while (wired_adapter.system_id <= WIRED_AUTO) {
        if (config.magic == CONFIG_MAGIC && config.global_cfg.system_cfg < WIRED_MAX
            && config.global_cfg.system_cfg != WIRED_AUTO) {
            break;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    detect_deinit();
#else
    wired_adapter.system_id = DC;
#endif

    if (wired_adapter.system_id >= 0) {
        printf("# Detected system : %d: %s\n", wired_adapter.system_id, sys_name[wired_adapter.system_id]);
    }

    while (config.magic != CONFIG_MAGIC) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    if (config.global_cfg.system_cfg < WIRED_MAX && config.global_cfg.system_cfg != WIRED_AUTO) {
        wired_adapter.system_id = config.global_cfg.system_cfg;
        printf("# Config override system : %d: %s\n", wired_adapter.system_id, sys_name[wired_adapter.system_id]);
    }

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        adapter_init_buffer(i);
    }

    if (wired_adapter.system_id < WIRED_MAX && wired_init[wired_adapter.system_id]) {
        wired_init[wired_adapter.system_id]();
    }
    vTaskDelete(NULL);
}

static void wl_init_task(void *arg) {
    err_led_clear();

    if (sd_init()) {
        err_led_set();
        printf("SD init fail!\n");
    }

    config_init();

    if (bt_host_init()) {
        err_led_set();
        printf("Bluetooth init fail!\n");
    }
    vTaskDelete(NULL);
}

void app_main()
{
    xTaskCreatePinnedToCore(wl_init_task, "wl_init_task", 2048, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(wired_init_task, "wired_init_task", 2048, NULL, 10, NULL, 1);
}

