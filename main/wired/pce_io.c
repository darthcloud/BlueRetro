/*
 * Copyright (c) 2021-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "adapter/wired/pce.h"
#include "system/core0_stall.h"
#include "system/delay.h"
#include "system/gpio.h"
#include "system/intr.h"
#include "pce_io.h"

#define GPIO_INTR_NUM 21

#define P1_SEL_PIN 33
#define P1_OE_PIN 26
#define P1_U_I_PIN 3
#define P1_R_II_PIN 5
#define P1_D_SL_PIN 18
#define P1_L_RN_PIN 23

#define P1_SEL_MASK (1 << (P1_SEL_PIN - 32))
#define P1_OE_MASK (1 << P1_OE_PIN)
#define P1_U_I_MASK (1 << P1_U_I_PIN)
#define P1_R_II_MASK (1 << P1_R_II_PIN)
#define P1_D_SL_MASK (1 << P1_D_SL_PIN)
#define P1_L_RN_MASK (1 << P1_L_RN_PIN)

#define POLL_TIMEOUT 512

#define P1_OUT0_MASK (BIT(P1_U_I_PIN) | BIT(P1_R_II_PIN) | BIT(P1_D_SL_PIN) | BIT(P1_L_RN_PIN))

#define PCE_OUT_DISABLE ~P1_OUT0_MASK

#define PCE_OUT_SET 0xFFFDFFFD
#define PCE_OUT_CLR (0xFFFDFFFD & PCE_OUT_DISABLE)

enum {
    DEV_NONE = 0,
    DEV_PCE_3BTNS,
    DEV_PCE_6BTNS,
    DEV_PCE_MOUSE,
    DEV_TYPE_MAX,
};

static uint32_t *map[] = {
    (uint32_t *)wired_adapter.data[0].output,
    (uint32_t *)wired_adapter.data[0].output,
    (uint32_t *)wired_adapter.data[0].output,
    (uint32_t *)wired_adapter.data[0].output,
    (uint32_t *)wired_adapter.data[0].output,
};
static uint32_t *map_mask[] = {
    (uint32_t *)wired_adapter.data[0].output_mask,
    (uint32_t *)wired_adapter.data[0].output_mask,
    (uint32_t *)wired_adapter.data[0].output_mask,
    (uint32_t *)wired_adapter.data[0].output_mask,
    (uint32_t *)wired_adapter.data[0].output_mask,
};
static uint8_t cycle = 0;
static uint8_t frame_cnt = 0;
static uint8_t mouse_cnt = 0;
static uint32_t axes[4] = {0};
static uint32_t axes_mask[4] = {
    P1_U_I_MASK, P1_R_II_MASK, P1_D_SL_MASK, P1_L_RN_MASK
};

static inline void load_mouse_axes() {
    uint8_t *relative = (uint8_t *)(wired_adapter.data[0].output + 12);
    int32_t *raw_axes = (int32_t *)(wired_adapter.data[0].output);
    int32_t val = 0;

    for (uint32_t i = 0; i < 2; i++) {
        if (relative[i]) {
            val = atomic_clear(&raw_axes[i]);
        }
        else {
            val = raw_axes[i];
        }

        if (val > 127) {
            axes[i * 2] = (P1_D_SL_MASK | P1_R_II_MASK | P1_U_I_MASK) | PCE_OUT_CLR;
            axes[i * 2 + 1] = (P1_L_RN_MASK | P1_D_SL_MASK | P1_R_II_MASK | P1_U_I_MASK) | PCE_OUT_CLR;
        }
        else if (val < -128) {
            axes[i * 2] = P1_L_RN_MASK | PCE_OUT_CLR;
            axes[i * 2 + 1] = PCE_OUT_CLR;
        }
        else {
            uint8_t val_tmp = (uint8_t)val;
            axes[i * 2] = PCE_OUT_CLR;
            axes[i * 2 + 1] = PCE_OUT_CLR;
            for (uint32_t j = 0, mask_l = 0x01, mask_m = 0x10; j < 4; ++j, mask_l <<= 1, mask_m <<= 1) {
                if (mask_m & val_tmp) {
                    axes[i * 2] |= axes_mask[j];
                }
                if (mask_l & val_tmp) {
                    axes[i * 2 + 1] |= axes_mask[j];
                }
            }
        }
    }
}

static void pce_ctrl_task(void) {
    uint32_t timeout, cur_in1, prev_in1, idx, change1 = 0, lock = 0;

    timeout = 0;
    cur_in1 = GPIO.in1.val;
    while (1) {
        prev_in1 = cur_in1;
        cur_in1 = GPIO.in1.val;
        while (!(change1 = cur_in1 ^ prev_in1)) {
            prev_in1 = cur_in1;
            cur_in1 = GPIO.in1.val;
            if (lock) {
                if (++timeout > POLL_TIMEOUT) {
                    core0_stall_end();
                    lock = 0;
                    timeout = 0;
                    ++wired_adapter.data[0].frame_cnt;
                    pce_gen_turbo_mask(&wired_adapter.data[0]);
                }
            }
        }

        if (change1 & P1_SEL_MASK) {
            if (!lock) {
                core0_stall_start();
                ++lock;
            }
            idx = ((cur_in1 & P1_SEL_MASK) >> 1) ^ 0x01;

            GPIO.out = map[0][idx] | map_mask[0][idx];
            timeout = 0;
        }
    }
}

static void pce_mt_task(void) {
    uint32_t timeout, cur_in1, prev_in1, change1 = 0, lock = 0;

    timeout = 0;
    cur_in1 = GPIO.in1.val;
    while (1) {
        prev_in1 = cur_in1;
        cur_in1 = GPIO.in1.val;
        while (!(change1 = cur_in1 ^ prev_in1)) {
            prev_in1 = cur_in1;
            cur_in1 = GPIO.in1.val;
            if (lock) {
                if (++timeout > POLL_TIMEOUT) {
                    core0_stall_end();
                    lock = 0;
                    timeout = 0;
                    for (uint32_t i = 0; i < 5; ++i) {
                        ++wired_adapter.data[i].frame_cnt;
                        pce_gen_turbo_mask(&wired_adapter.data[i]);
                    }
                }
            }
        }

        if (change1 & P1_SEL_MASK) {
            if (!lock) {
                core0_stall_start();
                ++lock;
            }
            if (cur_in1 & P1_SEL_MASK) {
                if (cycle < 4) {
                    ++cycle;
                }
                if (frame_cnt & 0x01) {
                    GPIO.out = PCE_OUT_CLR;
                }
                else {
                    GPIO.out = map[cycle][0] | map_mask[cycle][0];
                }
            }
            else {
                if (frame_cnt & 0x01) {
                    GPIO.out = map[cycle][2] | map_mask[cycle][2];
                }
                else {
                    GPIO.out = map[cycle][1] | map_mask[cycle][1];
                }
            }
            timeout = 0;
        }
    }
}

static unsigned pce_mt_oe_isr(unsigned cause) {
    uint32_t cur_in1 = GPIO.in1.val;
    cycle = 0;
    if (cur_in1 & P1_SEL_MASK) {
        GPIO.out = map[cycle][0] | map_mask[cycle][0];
    }
    else {
        GPIO.out = map[cycle][1] | map_mask[cycle][1];
    }
    GPIO.status_w1tc = P1_OE_MASK;
    return 0;
}

static unsigned pce_6btns_oe_isr(unsigned cause) {
    uint32_t cur_in1 = GPIO.in1.val;
    cycle = 0;
    ++frame_cnt;
    if (cur_in1 & P1_SEL_MASK) {
        if (frame_cnt & 0x01) {
            GPIO.out = PCE_OUT_CLR;
        }
        else {
            GPIO.out = map[cycle][0] | map_mask[cycle][0];
        }
    }
    else {
        if (frame_cnt & 0x01) {
            GPIO.out = map[cycle][2] | map_mask[cycle][2];
        }
        else {
            GPIO.out = map[cycle][1] | map_mask[cycle][1];
        }
    }
    GPIO.status_w1tc = P1_OE_MASK;
    return 0;
}

static void pce_mouse_task(void) {
    uint32_t timeout, cur_in1, prev_in1, change1 = 0, lock = 0;

    timeout = 0;
    cur_in1 = GPIO.in1.val;
    while (1) {
        prev_in1 = cur_in1;
        cur_in1 = GPIO.in1.val;
        while (!(change1 = cur_in1 ^ prev_in1)) {
            prev_in1 = cur_in1;
            cur_in1 = GPIO.in1.val;
            if (lock) {
                if (++timeout > POLL_TIMEOUT) {
                    core0_stall_end();
                    lock = 0;
                    timeout = 0;
                    mouse_cnt = 0;
                }
            }
        }

        if (change1 & P1_SEL_MASK) {
            if (!lock) {
                core0_stall_start();
                ++lock;
            }
            if (cur_in1 & P1_SEL_MASK) {
                if (cycle < 4) {
                    ++cycle;
                }
                if (mouse_cnt) {
                    GPIO.out = axes[mouse_cnt - 1];
                }
            }
            else {
                GPIO.out = map[0][2];
            }
            timeout = 0;
            if (mouse_cnt == 0) {
                load_mouse_axes();
            }
        }
    }
}

static unsigned pce_mouse_oe_isr(unsigned cause) {
    uint32_t cur_in1 = GPIO.in1.val;
    cycle = 0;
    ++frame_cnt;
    ++mouse_cnt;
    if (cur_in1 & P1_SEL_MASK) {
        if (mouse_cnt) {
            GPIO.out = axes[mouse_cnt - 1];
        }
    }
    else {
        GPIO.out = map[0][2];
    }
    GPIO.status_w1tc = P1_OE_MASK;
    return 0;
}

void pce_io_init(uint32_t package) {
    gpio_config_t io_conf = {0};

    /* SEL */
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = 1ULL << P1_SEL_PIN;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config_iram(&io_conf);

    /* OE */
    if (config.global_cfg.multitap_cfg == MT_SLOT_1 || config.out_cfg[0].dev_mode != DEV_PAD) {
        io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    }
    else {
        io_conf.intr_type = GPIO_INTR_DISABLE;
    }
    io_conf.pin_bit_mask = 1ULL << P1_OE_PIN;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config_iram(&io_conf);

    /* U, R, D, L */
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = 1ULL << P1_U_I_PIN;
    gpio_config_iram(&io_conf);
    io_conf.pin_bit_mask = 1ULL << P1_R_II_PIN;
    gpio_config_iram(&io_conf);
    io_conf.pin_bit_mask = 1ULL << P1_D_SL_PIN;
    gpio_config_iram(&io_conf);
    io_conf.pin_bit_mask = 1ULL << P1_L_RN_PIN;
    gpio_config_iram(&io_conf);
    gpio_set_level_iram(P1_U_I_PIN, 1);
    gpio_set_level_iram(P1_R_II_PIN, 1);
    gpio_set_level_iram(P1_D_SL_PIN, 1);
    gpio_set_level_iram(P1_L_RN_PIN, 1);

    if (config.global_cfg.multitap_cfg == MT_SLOT_1) {
        map[0] = (uint32_t *)wired_adapter.data[0].output;
        map[1] = (uint32_t *)wired_adapter.data[1].output;
        map[2] = (uint32_t *)wired_adapter.data[2].output;
        map[3] = (uint32_t *)wired_adapter.data[3].output;
        map[4] = (uint32_t *)wired_adapter.data[4].output;
        map_mask[0] = (uint32_t *)wired_adapter.data[0].output_mask;
        map_mask[1] = (uint32_t *)wired_adapter.data[1].output_mask;
        map_mask[2] = (uint32_t *)wired_adapter.data[2].output_mask;
        map_mask[3] = (uint32_t *)wired_adapter.data[3].output_mask;
        map_mask[4] = (uint32_t *)wired_adapter.data[4].output_mask;
        if (config.out_cfg[0].dev_mode == DEV_PAD_ALT) {
            intexc_alloc_iram(ETS_GPIO_INTR_SOURCE, GPIO_INTR_NUM, pce_6btns_oe_isr);
        }
        else {
            intexc_alloc_iram(ETS_GPIO_INTR_SOURCE, GPIO_INTR_NUM, pce_mt_oe_isr);
        }
        pce_mt_task();
    }

    switch (config.out_cfg[0].dev_mode) {
        case DEV_MOUSE:
            intexc_alloc_iram(ETS_GPIO_INTR_SOURCE, GPIO_INTR_NUM, pce_mouse_oe_isr);
            pce_mouse_task();
            break;
        case DEV_PAD_ALT:
            intexc_alloc_iram(ETS_GPIO_INTR_SOURCE, GPIO_INTR_NUM, pce_6btns_oe_isr);
            pce_mt_task();
            break;
        default:
            pce_ctrl_task();
            break;
    }
}
