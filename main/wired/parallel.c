/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "system/gpio.h"
#include "zephyr/types.h"
#include "tools/util.h"

static const uint8_t output_list[] = {
    3, 5, 16, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33
};

void parallel_io_init(void)
{
    gpio_config_t io_conf = {0};

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    for (uint32_t i = 0; i < ARRAY_SIZE(output_list); i++) {
        io_conf.pin_bit_mask = 1ULL << output_list[i];
        gpio_config_iram(&io_conf);
        gpio_set_level_iram(output_list[i], 1);
    }
}
