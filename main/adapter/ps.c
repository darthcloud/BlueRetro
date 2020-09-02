/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "../zephyr/types.h"
#include "../util.h"
#include "config.h"
#include "ps.h"

enum {
    PS_SELECT = 0,
    PS_L3,
    PS_R3,
    PS_START,
    PS_D_UP,
    PS_D_RIGHT,
    PS_D_DOWN,
    PS_D_LEFT,
    PS_L2,
    PS_R2,
    PS_L1,
    PS_R1,
    PS_T,
    PS_C,
    PS_X,
    PS_S,
};

const uint8_t ps_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    2,       3,       0,       1,       14,     15
};

const uint8_t ps_mouse_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       0,       1,       0,     1
};

const struct ctrl_meta ps_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x80, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x80, .polarity = 1},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0xFF},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0xFF},
};

const struct ctrl_meta ps_mouse_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128},
};

struct ps_map {
    uint16_t buttons;
    uint8_t axes[16];
    uint8_t analog_btn;
} __packed;

const uint32_t ps_mask[4] = {0xBB7F0FFF, 0x00000000, 0x00000000, 0x00000000};
const uint32_t ps_desc[4] = {0x000000FF, 0x00000000, 0x00000000, 0x00000000};
const uint32_t ps_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(PS_D_LEFT), BIT(PS_D_RIGHT), BIT(PS_D_DOWN), BIT(PS_D_UP),
    0, 0, 0, 0,
    BIT(PS_S), BIT(PS_C), BIT(PS_X), BIT(PS_T),
    BIT(PS_START), BIT(PS_SELECT), 0, 0,
    BIT(PS_L2), BIT(PS_L1), 0, BIT(PS_L3),
    BIT(PS_R2), BIT(PS_R1), 0, BIT(PS_R3),
};
const uint8_t ps_btns_idx[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    5, 4, 7, 6,
    0, 0, 0, 0,
    11, 9, 10, 8,
    0, 0, 0, 0,
    14, 12, 0, 0,
    15, 13, 0, 0,
};

const uint32_t ps_mouse_mask[4] = {0x110000F0, 0x00000000, 0x00000000, 0x00000000};
const uint32_t ps_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
const uint32_t ps_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(PS_L1), 0, 0, 0,
    BIT(PS_R1), 0, 0, 0,
};

void ps_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
        {
            break;
        }
        case DEV_MOUSE:
        {
            struct ps_map *map = (struct ps_map *)wired_data->output;

            memset((void *)map, 0, sizeof(*map));
            map->buttons = 0xFCFF;
            map->analog_btn = 0x00;
            for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
                map->axes[ps_axes_idx[i]] = ps_axes_meta[i].neutral;
            }
            break;
        }
        default:
        {
            struct ps_map *map = (struct ps_map *)wired_data->output;

            memset((void *)map, 0, sizeof(*map));
            map->buttons = 0xFFFF;
            map->analog_btn = 0x00;
            for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
                map->axes[ps_axes_idx[i]] = ps_axes_meta[i].neutral;
            }
            break;
        }
    }
}

void ps_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_KB:
                    goto exit_axes_loop;
                case DEV_MOUSE:
                    ctrl_data[i].mask = ps_mouse_mask;
                    ctrl_data[i].desc = ps_mouse_desc;
                    ctrl_data[i].axes[j].meta = &ps_mouse_axes_meta[j];
                    break;
                default:
                    ctrl_data[i].mask = ps_mask;
                    ctrl_data[i].desc = ps_desc;
                    ctrl_data[i].axes[j].meta = &ps_axes_meta[j];
                    break;
            }
        }
exit_axes_loop:
        ;
    }
}

void ps_ctrl_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct ps_map map_tmp;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~ps_btns_mask[i];
                if (ps_btns_idx[i]) {
                    map_tmp.axes[ps_btns_idx[i]] = 0xFF;
                }
            }
            else {
                map_tmp.buttons |= ps_btns_mask[i];
                if (ps_btns_idx[i]) {
                    map_tmp.axes[ps_btns_idx[i]] = 0x00;
                }
            }
        }
    }

    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_MT]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_MT]) {
            map_tmp.analog_btn |= 0x01;
        }
        else {
            map_tmp.analog_btn &= ~0x01;
        }
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & ps_desc[0])) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                map_tmp.axes[ps_axes_idx[i]] = 255;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                map_tmp.axes[ps_axes_idx[i]] = 0;
            }
            else {
                map_tmp.axes[ps_axes_idx[i]] = (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

void ps_mouse_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct ps_map map_tmp;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~ps_mouse_btns_mask[i];
            }
            else {
                map_tmp.buttons |= ps_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & ps_mouse_desc[0])) {
            int32_t tmp_val = ctrl_data->axes[i].value + (int8_t)map_tmp.axes[ps_mouse_axes_idx[i]];

            if (tmp_val > ctrl_data->axes[i].meta->size_max) {
                map_tmp.axes[ps_mouse_axes_idx[i]] = 127;
            }
            else if (tmp_val < ctrl_data->axes[i].meta->size_min) {
                map_tmp.axes[ps_mouse_axes_idx[i]] = -128;
            }
            else {
                map_tmp.axes[ps_mouse_axes_idx[i]] += (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

static void ps_kb_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
}

void ps_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
            ps_kb_from_generic(ctrl_data, wired_data);
            break;
        case DEV_MOUSE:
            ps_mouse_from_generic(ctrl_data, wired_data);
            break;
        case DEV_PAD:
        default:
            ps_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
}

void ps_fb_to_generic(int32_t dev_mode, uint8_t *raw_fb_data, uint32_t raw_fb_len, struct generic_fb *fb_data) {
    fb_data->wired_id = raw_fb_data[0];
    fb_data->state = raw_fb_data[1];
    fb_data->cycles = 0;
    fb_data->start = 0;
}
