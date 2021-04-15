/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "system/gpio.h"
#include "led.h"

#define D1M_LED_PIN 2
#define ERR_LED_PIN 17

static void led_z(uint64_t pin) {
    gpio_config_t io_conf = {0};

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = 1ULL << pin;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    gpio_config_iram(&io_conf);
}

static void led_low(uint64_t pin) {
    gpio_config_t io_conf = {0};

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = 1ULL << pin;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    gpio_set_level_iram(pin, 0);
    gpio_config_iram(&io_conf);
    gpio_set_level_iram(pin, 0);
}

void err_led_set(void) {
    led_z(ERR_LED_PIN);
}

void err_led_clear(void) {
    led_low(ERR_LED_PIN);
}

void d1m_led_set(void) {
    led_low(D1M_LED_PIN);
}

void d1m_led_clear(void) {
    led_z(D1M_LED_PIN);
}
