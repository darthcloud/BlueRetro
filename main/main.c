/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_ota_ops.h>
#include <esp32/rom/ets_sys.h>
#include "system/bare_metal_app_cpu.h"
#include "system/core0_stall.h"
#include "system/delay.h"
#include "system/fpga_config.h"
#include "system/fs.h"
#include "system/led.h"
#include "adapter/adapter.h"
#include "adapter/adapter_debug.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "wired/detect.h"
#include "wired/wired_comm.h"
#include "adapter/memory_card.h"
#include "system/manager.h"
#include "sdkconfig.h"

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
        ets_printf("# Detected system : %d: %s\n", wired_adapter.system_id, wired_get_sys_name());
    }
#else
    wired_adapter.system_id = HARDCODED_SYS;
    ets_printf("# Hardcoded system : %d: %s\n", wired_adapter.system_id, wired_get_sys_name());
#endif

    while (config.magic != CONFIG_MAGIC) {
        delay_us(1000);
    }

    if (config.global_cfg.system_cfg < WIRED_MAX && config.global_cfg.system_cfg != WIRED_AUTO) {
        wired_adapter.system_id = config.global_cfg.system_cfg;
        ets_printf("# Config override system : %d: %s\n", wired_adapter.system_id, wired_get_sys_name());
    }

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        adapter_init_buffer(i);
    }

    struct raw_fb fb_data = {0};
    const char *sysname = wired_get_sys_name();
    fb_data.header.wired_id = 0;
    fb_data.header.type = FB_TYPE_SYS_ID;
    fb_data.header.data_len = strlen(sysname);
    memcpy(fb_data.data, sysname, fb_data.header.data_len);
    adapter_q_fb(&fb_data);

    if (wired_adapter.system_id < WIRED_MAX) {
        wired_comm_init();
    }
}

static void wl_init_task(void *arg) {
    uint32_t err = 0;
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    esp_ota_get_state_partition(running, &ota_state);
    err_led_init();

    core0_stall_init();

    if (fs_init()) {
        err_led_set();
        err = 1;
        printf("FS init fail!\n");
    }

#ifdef CONFIG_BLUERETRO_SYSTEM_SEA_BOARD
    fpga_config();
#endif

    config_init(DEFAULT_CFG);

#ifndef CONFIG_BLUERETRO_BT_DISABLE
    if (bt_host_init()) {
        err_led_set();
        err = 1;
        printf("Bluetooth init fail!\n");
    }
#endif

    mc_init();

    sys_mgr_init();

#ifdef CONFIG_BLUERETRO_PKT_INJECTION
    adapter_debug_injector_init();
#endif

    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        if (err) {
            printf("Boot error on pending img, rollback to the previous version.");
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
        else {
            printf("Pending img valid!");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
    vTaskDelete(NULL);
}

void app_main()
{
    adapter_init();

    start_app_cpu(wired_init_task);
    xTaskCreatePinnedToCore(wl_init_task, "wl_init_task", 2560, NULL, 10, NULL, 0);
}

