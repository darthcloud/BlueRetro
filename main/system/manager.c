/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_partition.h>
#include <esp_sleep.h>
#include "driver/gpio.h"
#include "adapter/adapter.h"
#include "bluetooth/host.h"
#include "bluetooth/hci.h"
#include "wired/wired_comm.h"
#include "tools/util.h"
#include "system/fs.h"
#include "manager.h"

#define RESET_PIN 14

#define POWER_ON_PIN 13
#define POWER_OFF_PIN 12
#define POWER_SENSE_PIN 39

#define SENSE_P1_PIN 4
#define SENSE_P2_PIN 15
#define SENSE_P3_PIN 2
#define SENSE_P4_PIN 0

#if defined(CONFIG_BLUERETRO_SYSTEM_N64) || defined(CONFIG_BLUERETRO_SYSTEM_DC) || defined(CONFIG_BLUERETRO_SYSTEM_GC)
#define PORT_CNT 4
#else
#define PORT_CNT 2
#endif

enum {
    SYS_MGR_POWER_ON_HOLD = 0,
};

static const uint8_t input_list[] = {
    SENSE_P1_PIN, SENSE_P2_PIN, SENSE_P3_PIN, SENSE_P4_PIN
};
static atomic_t sys_mgr_flags = 0;

static void power_on_hdl(void) {
    static int32_t curr = 0, prev = 0;
    struct bt_dev *not_used;

    prev = curr;
    curr = gpio_get_level(POWER_SENSE_PIN);
    if (curr) {
        /* System is power on */
        bt_hci_inquiry_override(0);

        if (bt_host_get_active_dev(&not_used) < 0) {
            /* No Bt device */
            uint32_t port_cnt = 0;

            for (uint32_t i = 0; i < PORT_CNT; i++) {
                if (!gpio_get_level(input_list[i])) {
                    port_cnt++;
                }
            }
            if (port_cnt == PORT_CNT) {
                /* No wired controller */
                if (!bt_hci_get_inquiry()) {
                    bt_hci_start_inquiry();
                }
            }
        }
    }
    else {
        /* System is off */
        if (curr != prev) {
            /* Kick BT controllers on off transition */
            bt_host_disconnect_all();
        }
        else if (bt_host_get_active_dev(&not_used) > -1) {
            /* Power on BT connect */
            bt_hci_inquiry_override(0);
            gpio_set_level(POWER_ON_PIN, 0);
            if (!atomic_test_bit(&sys_mgr_flags, SYS_MGR_POWER_ON_HOLD)) {
                vTaskDelay(500 / portTICK_PERIOD_MS);
                gpio_set_level(POWER_ON_PIN, 1);
            }
            return;
        }
        /* Make sure no inquiry while power off */
        bt_hci_inquiry_override(1);
        if (bt_hci_get_inquiry()) {
            bt_hci_stop_inquiry();
        }
    }
}

static void wired_port_hdl(void) {
    uint32_t update = 0;
    uint16_t port_mask = 0;
    for (int32_t i = 0, j = 0, idx = 0; i < BT_MAX_DEV; i++) {
        struct bt_dev *device = NULL;
        int32_t prev_idx = 0;

        bt_host_get_dev_from_id(i, &device);
        prev_idx = device->ids.out_idx;

        for (; j < PORT_CNT; j++) {
            if (!gpio_get_level(input_list[j])) {
                j++;
                break;
            }
            idx++;
        }
        port_mask |= BIT(idx);
        device->ids.out_idx = idx++;

        if (device->ids.out_idx != prev_idx) {
            update++;
            printf("# %s: BTDEV %ld map to WIRED %ld\n", __FUNCTION__, i, device->ids.out_idx);
            if (atomic_test_bit(&device->flags, BT_DEV_HID_INTR_READY)) {
                struct raw_fb fb_data = {0};

                fb_data.header.wired_id = device->ids.out_idx;
                fb_data.header.type = FB_TYPE_PLAYER_LED;
                fb_data.header.data_len = 0;
                adapter_q_fb(&fb_data);
            }
        }
    }
    if (update) {
        printf("# %s: Update ports state: %04X\n", __FUNCTION__, port_mask);
        wired_comm_port_cfg(port_mask);
#ifdef CONFIG_BLUERETRO_SYSTEM_WII_EXT
        /* Toggle Wii classic sense line to force ctrl reinit */
        gpio_set_level(SENSE_P3_PIN, 0);
        gpio_set_level(SENSE_P4_PIN, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(SENSE_P3_PIN, 1);
        gpio_set_level(SENSE_P4_PIN, 1);
#endif
    }
}

static void sys_mgr_task(void *arg) {
    while (1) {
        wired_port_hdl();
        power_on_hdl();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void sys_mgr_reset(void) {
    gpio_set_level(RESET_PIN, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(RESET_PIN, 1);
}

void sys_mgr_inquiry(void) {
    if (bt_hci_get_inquiry()) {
        bt_hci_stop_inquiry();
    }
    else {
        bt_hci_start_inquiry();
    }
}

void sys_mgr_power_off(void) {
    bt_host_disconnect_all();
    gpio_set_level(POWER_ON_PIN, 1);
}

void sys_mgr_factory_reset(void) {
    const esp_partition_t* partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, "otadata");
    if (partition) {
        esp_partition_erase_range(partition, 0, partition->size);
    }

    fs_reset();
    printf("BlueRetro factory reset\n");
    bt_host_disconnect_all();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
}

void sys_mgr_deep_sleep(void) {
    esp_deep_sleep_start();
}

void sys_mgr_init(void) {
    gpio_config_t io_conf = {0};

#ifdef CONFIG_BLUERETRO_SYSTEM_WII_EXT
    sys_mgr_flags = BIT(SYS_MGR_POWER_ON_HOLD);
#endif

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    for (uint32_t i = 0; i < PORT_CNT; i++) {
        io_conf.pin_bit_mask = 1ULL << input_list[i];
        gpio_config(&io_conf);
    }
    io_conf.pin_bit_mask = 1ULL << POWER_SENSE_PIN;
    gpio_config(&io_conf);

    gpio_set_level(POWER_ON_PIN, 1);
    io_conf.mode = GPIO_MODE_OUTPUT_OD;
    io_conf.pin_bit_mask = 1ULL << POWER_ON_PIN;
    gpio_config(&io_conf);

    gpio_set_level(RESET_PIN, 1);
    io_conf.pin_bit_mask = 1ULL << RESET_PIN;
    gpio_config(&io_conf);

#ifdef CONFIG_BLUERETRO_SYSTEM_WII_EXT
    /* Wii-ext got a sense line that we need to control */
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = 1ULL << SENSE_P4_PIN;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpio_set_level(SENSE_P4_PIN, 1);
    io_conf.pin_bit_mask = 1ULL << SENSE_P3_PIN;
    gpio_config(&io_conf);
    gpio_set_level(SENSE_P3_PIN, 1);
#endif

    xTaskCreatePinnedToCore(sys_mgr_task, "sys_mgr_task", 2048, NULL, 5, NULL, 0);
}

