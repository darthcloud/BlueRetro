/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "adapter/wired/wired.h"
#include "wii.h"

enum {
    WII_CLASSIC_R = 1,
    WII_CLASSIC_PLUS,
    WII_CLASSIC_HOME,
    WII_CLASSIC_MINUS,
    WII_CLASSIC_L,
    WII_CLASSIC_D_DOWN,
    WII_CLASSIC_D_RIGHT,
    WII_CLASSIC_D_UP,
    WII_CLASSIC_D_LEFT,
    WII_CLASSIC_ZR,
    WII_CLASSIC_X,
    WII_CLASSIC_A,
    WII_CLASSIC_Y,
    WII_CLASSIC_B,
    WII_CLASSIC_ZL,
};

static DRAM_ATTR const struct ctrl_meta wiic_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x66},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x66},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x66},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x66},
    {.size_min = 0, .size_max = 255, .neutral = 0x16, .abs_max = 0xDA},
    {.size_min = 0, .size_max = 255, .neutral = 0x16, .abs_max = 0xDA},
};

struct wiic_map {
    uint8_t axes[6];
    uint16_t buttons;
} __packed;

static DRAM_ATTR const uint8_t wiic_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       2,       1,       3,       4,      5
};

static const uint32_t wiic_mask[4] = {0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t wiic_pro_desc[4] = {0x000000FF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t wiic_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t wiic_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(WII_CLASSIC_D_LEFT), BIT(WII_CLASSIC_D_RIGHT), BIT(WII_CLASSIC_D_DOWN), BIT(WII_CLASSIC_D_UP),
    0, 0, 0, 0,
    BIT(WII_CLASSIC_Y), BIT(WII_CLASSIC_A), BIT(WII_CLASSIC_B), BIT(WII_CLASSIC_X),
    BIT(WII_CLASSIC_PLUS), BIT(WII_CLASSIC_MINUS), BIT(WII_CLASSIC_HOME), 0,
    BIT(WII_CLASSIC_L), BIT(WII_CLASSIC_ZL), BIT(WII_CLASSIC_L), 0,
    BIT(WII_CLASSIC_R), BIT(WII_CLASSIC_ZR), BIT(WII_CLASSIC_R), 0,
};

void IRAM_ATTR wii_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        default:
        {
            struct wiic_map *map = (struct wiic_map *)wired_data->output;

            memset(wired_data->output, 0, 32);
            map->buttons = 0xFFFF;
            for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
                map->axes[wiic_axes_idx[i]] = wiic_axes_meta[i].neutral;
            }
            memset(wired_data->output_mask, 0x00, sizeof(struct wiic_map));
            break;
        }
    }
}

void wii_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*4);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_PAD_ALT:
                    ctrl_data[i].mask = wiic_mask;
                    ctrl_data[i].desc = wiic_desc;
                    ctrl_data[i].axes[j].meta = &wiic_axes_meta[j];
                    break;
                default:
                    ctrl_data[i].mask = wiic_mask;
                    ctrl_data[i].desc = wiic_pro_desc;
                    ctrl_data[i].axes[j].meta = &wiic_axes_meta[j];
                    break;
            }
        }
    }
}

void wii_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct wiic_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & generic_btns_mask[i]) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~wiic_btns_mask[i];
                map_mask &= ~wiic_btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else if (map_mask & wiic_btns_mask[i]) {
                map_tmp.buttons |= wiic_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    if (dev_mode == DEV_PAD) {
        /* Wii Classic Pro analogs status is link to digital buttons */
        if (map_tmp.buttons & BIT(WII_CLASSIC_L)) {
            map_tmp.axes[wiic_axes_idx[TRIG_L]] = 0x00;
        }
        else {
            map_tmp.axes[wiic_axes_idx[TRIG_L]] = 0xF8;
        }
        if (map_tmp.buttons & BIT(WII_CLASSIC_R)) {
            map_tmp.axes[wiic_axes_idx[TRIG_R]] = 0x00;
        }
        else {
            map_tmp.axes[wiic_axes_idx[TRIG_R]] = 0xF8;
        }
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & ctrl_data->desc[0])) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                map_tmp.axes[wiic_axes_idx[i]] = 255;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                map_tmp.axes[wiic_axes_idx[i]] = 0;
            }
            else {
                map_tmp.axes[wiic_axes_idx[i]] = (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
        wired_data->cnt_mask[axis_to_btn_id(i)] = ctrl_data->axes[i].cnt_mask;
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

void IRAM_ATTR wii_gen_turbo_mask(struct wired_data *wired_data) {
    struct wiic_map *map_mask = (struct wiic_map *)wired_data->output_mask;

    memset(map_mask, 0x00, sizeof(*map_mask));

    wired_gen_turbo_mask_btns16_neg(wired_data, &map_mask->buttons, wiic_btns_mask);
    wired_gen_turbo_mask_axes8(wired_data, map_mask->axes, ADAPTER_MAX_AXES, wiic_axes_idx, wiic_axes_meta);
}
