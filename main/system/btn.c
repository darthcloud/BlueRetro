/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "zephyr/atomic.h"
#include "driver/gpio.h"
#include "system/fs.h"
#include "btn.h"

#define BOOT_BTN_PIN 0
#define HOLD_EVT_THRESHOLD 300
#define RESET_EVT_THRESHOLD 1000

/* Button flags */
enum {
    HOLD_EVT_SET = 0,
};

static atomic_t btn_flags = 0;
static TaskHandle_t boot_btn_task_hdl;
static void (*boot_btn_cb[BOOT_BTN_MAX_EVT])(void) = {0};

static void boot_btn_task(void *param) {
    while (1) {
        uint32_t hold_cnt = 0;
        if (!gpio_get_level(BOOT_BTN_PIN)) {
            while (!gpio_get_level(BOOT_BTN_PIN)) {
                if (hold_cnt++ > HOLD_EVT_THRESHOLD) {
                    break;
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }

            if (hold_cnt > HOLD_EVT_THRESHOLD) {
                if (boot_btn_cb[BOOT_BTN_HOLD_EVT]) {
                    boot_btn_cb[BOOT_BTN_HOLD_EVT]();
                }
            }
            else if (atomic_test_bit(&btn_flags, HOLD_EVT_SET)) {
                if (boot_btn_cb[BOOT_BTN_HOLD_CANCEL_EVT]) {
                    boot_btn_cb[BOOT_BTN_HOLD_CANCEL_EVT]();
                }
            }
            else {
                if (boot_btn_cb[BOOT_BTN_DEFAULT_EVT]) {
                    boot_btn_cb[BOOT_BTN_DEFAULT_EVT]();
                }
            }

            while (!gpio_get_level(BOOT_BTN_PIN)) {
                if (hold_cnt++ > RESET_EVT_THRESHOLD) {
                    fs_reset();
                    printf("BlueRetro factory reset\n");
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    esp_restart();
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }

            /* Inhibit SW press for 2 seconds */
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void boot_btn_init(void) {
    gpio_config_t io_conf = {0};

    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.pin_bit_mask = 1ULL << BOOT_BTN_PIN;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    xTaskCreatePinnedToCore(&boot_btn_task, "boot_btn_task", 2048, NULL, 5, &boot_btn_task_hdl, 0);
}

void boot_btn_set_callback(void (*cb)(void), boot_btn_evt_t evt) {
    boot_btn_cb[evt] = cb;
}

void boot_btn_hold_state(uint32_t state) {
    if (state) {
        atomic_set_bit(&btn_flags, HOLD_EVT_SET);
    }
    else {
        atomic_clear_bit(&btn_flags, HOLD_EVT_SET);
    }
}
