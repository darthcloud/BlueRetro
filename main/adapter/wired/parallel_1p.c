/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "adapter/config.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "parallel_1p.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#define P1_LD_UP 3
#define P1_LD_DOWN 5
#define P1_LD_LEFT 18
#define P1_LD_RIGHT 23
#define P1_RB_DOWN 26
#define P1_RB_RIGHT 27
#define P1_RB_LEFT 19
#define P1_RB_UP 21
#define P1_MM 32
#define P1_MS 22
#define P1_MT 16
#define P1_LM 25
#define P1_RM 33

static const uint32_t para_1p_mask[4] = {0x337F0F00, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t para_1p_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t para_1p_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(P1_LD_LEFT), BIT(P1_LD_RIGHT), BIT(P1_LD_DOWN), BIT(P1_LD_UP),
    0, 0, 0, 0,
    BIT(P1_RB_LEFT), BIT(P1_RB_RIGHT), BIT(P1_RB_DOWN), BIT(P1_RB_UP),
    BIT(P1_MM - 32) | 0xF0000000, BIT(P1_MS), BIT(P1_MT), 0,
    BIT(P1_LM), BIT(P1_LM), 0, 0,
    BIT(P1_RM - 32) | 0xF0000000, BIT(P1_RM - 32) | 0xF0000000, 0, 0,
};

void IRAM_ATTR para_1p_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    struct para_1p_map *map = (struct para_1p_map *)wired_data->output;
    struct para_1p_map *map_mask = (struct para_1p_map *)wired_data->output_mask;

    map->buttons = 0xFFFDFFFF;
    map->buttons_high = 0xFFFFFFFF;
    map_mask->buttons = 0;
    map_mask->buttons_high = 0;
}

void para_1p_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        ctrl_data[i].mask = para_1p_mask;
        ctrl_data[i].desc = para_1p_desc;
    }
}

void para_1p_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    if (ctrl_data->index < 1) {
        struct para_1p_map map_tmp;
        uint32_t map_mask = 0xFFFFFFFF;
        uint32_t map_mask_high = 0xFFFFFFFF;
        struct para_1p_map *turbo_map_mask = (struct para_1p_map *)wired_data->output_mask;

        memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (ctrl_data->map_mask[0] & BIT(i)) {
                if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                    if ((para_1p_btns_mask[i] & 0xF0000000) == 0xF0000000) {
                        map_tmp.buttons_high &= ~(para_1p_btns_mask[i] & 0x000000FF);
                        map_mask_high &= ~(para_1p_btns_mask[i] & 0x000000FF);
                    }
                    else {
                        map_tmp.buttons &= ~para_1p_btns_mask[i];
                        map_mask &= ~para_1p_btns_mask[i];
                    }
                    wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
                }
                else {
                    if ((para_1p_btns_mask[i] & 0xF0000000) == 0xF0000000) {
                        if (map_mask & (para_1p_btns_mask[i] & 0x000000FF)) {
                            map_tmp.buttons_high |= para_1p_btns_mask[i] & 0x000000FF;
                        }
                    }
                    else {
                        if (map_mask & para_1p_btns_mask[i]) {
                            map_tmp.buttons |= para_1p_btns_mask[i];
                        }
                    }
                    wired_data->cnt_mask[i] = 0;
                }
            }
        }

        GPIO.out = map_tmp.buttons | turbo_map_mask->buttons;
        GPIO.out1.val = map_tmp.buttons_high | turbo_map_mask->buttons_high;

        memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
        printf("{\"log_type\": \"wired_output\", \"btns\": [%ld, %ld]}\n",
            map_tmp.buttons, map_tmp.buttons_high);
#endif
    }
}

void para_1p_gen_turbo_mask(struct wired_data *wired_data) {
    struct para_1p_map *map_mask = (struct para_1p_map *)wired_data->output_mask;

    memset(map_mask, 0, sizeof(*map_mask));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        uint8_t mask = wired_data->cnt_mask[i] >> 1;

        if (mask) {
            if (para_1p_btns_mask[i]) {
                if (wired_data->cnt_mask[i] & 1) {
                    if (!(mask & wired_data->frame_cnt)) {
                        if ((para_1p_btns_mask[i] & 0xF0000000) == 0xF0000000) {
                            map_mask->buttons_high |= para_1p_btns_mask[i];
                        }
                        else {
                            map_mask->buttons |= para_1p_btns_mask[i];
                        }
                    }
                }
                else {
                    if (!((mask & wired_data->frame_cnt) == mask)) {
                        if ((para_1p_btns_mask[i] & 0xF0000000) == 0xF0000000) {
                            map_mask->buttons_high |= para_1p_btns_mask[i];
                        }
                        else {
                            map_mask->buttons |= para_1p_btns_mask[i];
                        }
                    }
                }
            }
        }
    }
}
