/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "../zephyr/types.h"
#include "../util.h"
#include "saturn.h"

enum {
    SATURN_B = 0,
    SATURN_C,
    SATURN_A,
    SATURN_START,
    SATURN_LD_UP,
    SATURN_LD_DOWN,
    SATURN_LD_LEFT,
    SATURN_LD_RIGHT,
    SATURN_L = 11,
    SATURN_Z,
    SATURN_Y,
    SATURN_X,
    SATURN_R,
};

static const uint8_t saturn_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       0,       0,       3,      2
};

static const struct ctrl_meta saturn_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x80, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x80}, //NA
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x80, .polarity = 1}, //NA
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0xFF},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0xFF},
};

struct saturn_map {
    uint16_t buttons;
    uint8_t axes[4];
} __packed;

static const uint32_t saturn_mask[4] = {0xBB1F0F0F, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t saturn_desc[4] = {0x1100000F, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t saturn_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(SATURN_LD_LEFT), BIT(SATURN_LD_RIGHT), BIT(SATURN_LD_DOWN), BIT(SATURN_LD_UP),
    0, 0, 0, 0,
    BIT(SATURN_X), BIT(SATURN_B), BIT(SATURN_A), BIT(SATURN_Y),
    BIT(SATURN_START), 0, 0, 0,
    0, BIT(SATURN_Z), 0, BIT(SATURN_L),
    0, BIT(SATURN_C), 0, BIT(SATURN_R),
};

void saturn_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    struct saturn_map *map = (struct saturn_map *)wired_data->output;

    map->buttons = 0xFFFF;
    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (i == 2 || i == 3) {
            continue;
        }
        map->axes[saturn_axes_idx[i]] = saturn_axes_meta[i].neutral;
    }
}

void saturn_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        ctrl_data[i].mask = saturn_mask;
        ctrl_data[i].desc = saturn_desc;
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            ctrl_data[i].axes[j].meta = &saturn_axes_meta[j];
        }
    }
}

void saturn_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct saturn_map map_tmp;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~saturn_btns_mask[i];
            }
            else {
                map_tmp.buttons |= saturn_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (i == 2 || i == 3) {
            continue;
        }
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & saturn_desc[0])) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                map_tmp.axes[saturn_axes_idx[i]] = 255;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                map_tmp.axes[saturn_axes_idx[i]] = 0;
            }
            else {
                map_tmp.axes[saturn_axes_idx[i]] = (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}
