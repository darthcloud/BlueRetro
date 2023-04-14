/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "system/intr.h"
#include "system/gpio.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "detect.h"

#define ALT_SYS_PIN 39

static const uint8_t pins[] = {
    19, 21, 22, 25, 32, 33, 34, 35, ALT_SYS_PIN,
};
static const uint8_t *detect_pin_low = &pins[0];
static const uint8_t *detect_pin_high = &pins[4];

static const uint8_t system_id_low[][4] = {
    {N64, CDI, DC, WII_EXT},
    {GC, CDI, REAL_3DO, WII_EXT},
};

static const uint8_t system_id_high[][4] = {
    {NES, PCE, PSX, GENESIS},
    {SNES, PCFX, PS2, SATURN},
};

static void detect_system(const uint32_t io, const uint8_t (*system_id)[4], const uint8_t *detect_pin) {
    if (wired_adapter.system_id <= WIRED_AUTO) {
        for (uint32_t i = 0; i < 4; i++) {
            if (io & BIT(detect_pin[i] - 32)) {
                if (GPIO.in1.val & BIT(ALT_SYS_PIN - 32)) {
                    wired_adapter.system_id = system_id[0][i];
                }
                else {
                    wired_adapter.system_id = system_id[1][i];
                }
            }
        }
    }
}

static unsigned detect_isr(unsigned cause) {
    const uint32_t low_io = GPIO.acpu_int;
    const uint32_t high_io = GPIO.acpu_int1.intr;

    if (high_io) {
        detect_system(high_io, system_id_high, detect_pin_high);
        GPIO.status1_w1tc.intr_st = high_io;
    }
    if (low_io) {
        detect_system(low_io, system_id_low, detect_pin_low);
        GPIO.status_w1tc = low_io;
    }
    return 0;
}

void detect_init(void) {
    static gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };

    for (uint32_t i = 0; i < ARRAY_SIZE(pins); i++) {
        io_conf.intr_type = (pins[i] == 39) ? GPIO_INTR_DISABLE : GPIO_INTR_ANYEDGE;
        io_conf.pin_bit_mask = 1ULL << pins[i];
        gpio_config_iram(&io_conf);
    }

    wired_adapter.system_id = WIRED_AUTO;

    adapter_init_buffer(0);

    intexc_alloc_iram(ETS_GPIO_INTR_SOURCE, 19, detect_isr);
}

void detect_deinit(void) {
    intexc_free_iram(ETS_GPIO_INTR_SOURCE, 19);

    for (uint32_t i = 0; i < ARRAY_SIZE(pins); i++) {
        gpio_reset_iram(pins[i]);
    }
}
