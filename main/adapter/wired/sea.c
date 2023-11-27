/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "adapter/config.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "wired/sea_io.h"
#include "sea.h"

#define P1_LD_UP 19
#define P1_LD_DOWN 22
#define P1_LD_LEFT 18
#define P1_LD_RIGHT 21
#define P1_RB_DOWN 5
#define P1_RB_RIGHT 13
#define P1_RB_LEFT 23
#define P1_RB_UP 12
#define P1_MM 16
#define P1_MS 4
#define P1_MT 33
#define P1_LM 15
#define P1_RM 14

static const uint32_t sea_mask[4] = {0x337F0F00, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t sea_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t sea_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(P1_LD_LEFT), BIT(P1_LD_RIGHT), BIT(P1_LD_DOWN), BIT(P1_LD_UP),
    0, 0, 0, 0,
    BIT(P1_RB_LEFT), BIT(P1_RB_RIGHT), BIT(P1_RB_DOWN), BIT(P1_RB_UP),
    BIT(P1_MM), BIT(P1_MS), BIT(P1_MT - 32) | 0xF0000000, 0,
    BIT(P1_LM), BIT(P1_LM), 0, 0,
    BIT(P1_RM), BIT(P1_RM), 0, 0,
};
static const uint32_t sea_gbahd_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(GBAHD_LD_LEFT), BIT(GBAHD_LD_RIGHT), BIT(GBAHD_LD_DOWN), BIT(GBAHD_LD_UP),
    0, 0, 0, 0,
    BIT(GBAHD_B), 0, BIT(GBAHD_A), 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
};

void IRAM_ATTR sea_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    struct sea_map *map = (struct sea_map *)wired_data->output;
    struct sea_map *map_mask = (struct sea_map *)wired_data->output_mask;

    map->buttons = 0xFFFDFFFF;
    map->buttons_high = 0xFFFFFFFF;
    map->buttons_osd = 0x00;
    map_mask->buttons = 0;
    map_mask->buttons_high = 0;
}

void sea_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        ctrl_data[i].mask = sea_mask;
        ctrl_data[i].desc = sea_desc;
    }
}

void sea_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    if (ctrl_data->index < 1) {
        struct sea_map map_tmp;
        uint32_t map_mask = 0xFFFFFFFF;
        uint32_t map_mask_high = 0xFFFFFFFF;
        struct sea_map *turbo_map_mask = (struct sea_map *)wired_data->output_mask;

        memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (ctrl_data->map_mask[0] & BIT(i)) {
                if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                    if ((sea_btns_mask[i] & 0xF0000000) == 0xF0000000) {
                        map_tmp.buttons_high &= ~(sea_btns_mask[i] & 0x000000FF);
                        map_mask_high &= ~(sea_btns_mask[i] & 0x000000FF);
                    }
                    else {
                        map_tmp.buttons &= ~sea_btns_mask[i];
                        map_mask &= ~sea_btns_mask[i];
                    }
                    map_tmp.buttons_osd |= sea_gbahd_btns_mask[i];
                    wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
                }
                else {
                    if ((sea_btns_mask[i] & 0xF0000000) == 0xF0000000) {
                        if (map_mask & (sea_btns_mask[i] & 0x000000FF)) {
                            map_tmp.buttons_high |= sea_btns_mask[i] & 0x000000FF;
                        }
                    }
                    else {
                        if (map_mask & sea_btns_mask[i]) {
                            map_tmp.buttons |= sea_btns_mask[i];
                        }
                    }
                    map_tmp.buttons_osd &= ~sea_gbahd_btns_mask[i];
                    wired_data->cnt_mask[i] = 0;
                }
            }
        }

        if (!(map_tmp.buttons_osd & BIT(GBAHD_OSD))) {
            GPIO.out = map_tmp.buttons | turbo_map_mask->buttons;
            GPIO.out1.val = map_tmp.buttons_high | turbo_map_mask->buttons_high;
        }

        /* OSD buttons */
        if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_MT]) {
            if (ctrl_data->btns[0].value & generic_btns_mask[PAD_MT]) {
                if (!atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                    atomic_set_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);
                }
            }
            else {
                if (atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                    atomic_clear_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);

                    map_tmp.buttons_osd ^= BIT(GBAHD_OSD);
                }
            }
        }

        sea_tx_byte(map_tmp.buttons_osd);

        memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
        printf("{\"log_type\": \"wired_output\", \"btns\": [%ld, %ld, %d]}\n",
            map_tmp.buttons, map_tmp.buttons_high, map_tmp.buttons_osd);
#endif
    }
}

void sea_gen_turbo_mask(struct wired_data *wired_data) {
    struct sea_map *map_mask = (struct sea_map *)wired_data->output_mask;

    memset(map_mask, 0, sizeof(*map_mask));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        uint8_t mask = wired_data->cnt_mask[i] >> 1;

        if (mask) {
            if (sea_btns_mask[i]) {
                if (wired_data->cnt_mask[i] & 1) {
                    if (!(mask & wired_data->frame_cnt)) {
                        if ((sea_btns_mask[i] & 0xF0000000) == 0xF0000000) {
                            map_mask->buttons_high |= sea_btns_mask[i];
                        }
                        else {
                            map_mask->buttons |= sea_btns_mask[i];
                        }
                    }
                }
                else {
                    if (!((mask & wired_data->frame_cnt) == mask)) {
                        if ((sea_btns_mask[i] & 0xF0000000) == 0xF0000000) {
                            map_mask->buttons_high |= sea_btns_mask[i];
                        }
                        else {
                            map_mask->buttons |= sea_btns_mask[i];
                        }
                    }
                }
            }
        }
    }
}
