/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "../zephyr/types.h"
#include "../util.h"
#include "n64.h"

#define N64_AXES_MAX 2
enum {
    N64_LD_RIGHT,
    N64_LD_LEFT,
    N64_LD_DOWN,
    N64_LD_UP,
    N64_START,
    N64_Z,
    N64_B,
    N64_A,
    N64_C_RIGHT,
    N64_C_LEFT,
    N64_C_DOWN,
    N64_C_UP,
    N64_R,
    N64_L,
};

const uint8_t n64_axes_idx[N64_AXES_MAX] =
{
/*  AXIS_LX, AXIS_LY  */
    0,       1
};

const struct ctrl_meta n64_btns_meta =
{
    .polarity = 0,
};

const struct ctrl_meta n64_axes_meta[N64_AXES_MAX] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x54},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x54},
};

const struct ctrl_meta n64_mouse_axes_meta[N64_AXES_MAX] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x80},
};

struct n64_map {
    uint16_t buttons;
    uint8_t axes[2];
} __packed;

struct n64_kb_map {
    uint16_t key_codes[3];
    uint8_t bitfield;
} __packed;

const uint32_t n64_mask[4] = {0x331F0FFF, 0x00000000, 0x00000000, 0x00000000};
const uint32_t n64_desc[4] = {0x0000000F, 0x00000000, 0x00000000, 0x00000000};
const uint32_t n64_btns_mask[32] = {
    0, 0, 0, 0,
    BIT(N64_C_LEFT), BIT(N64_C_RIGHT), BIT(N64_C_DOWN), BIT(N64_C_UP),
    BIT(N64_LD_LEFT), BIT(N64_LD_RIGHT), BIT(N64_LD_DOWN), BIT(N64_LD_UP),
    0, 0, 0, 0,
    BIT(N64_B), BIT(N64_C_DOWN), BIT(N64_A), BIT(N64_C_LEFT),
    BIT(N64_START), 0, 0, 0,
    BIT(N64_Z), BIT(N64_L), 0, 0,
    BIT(N64_Z), BIT(N64_R), 0, 0,
};

const uint32_t n64_mouse_mask[4] = {0x110000F0, 0x00000000, 0x00000000, 0x00000000};
const uint32_t n64_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
const uint32_t n64_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(N64_B), 0, 0, 0,
    BIT(N64_A), 0, 0, 0,
};

const uint32_t n64_kb_mask[4] = {0xE6FF0F0F, 0xFFFFFFFF, 0x2D7FFFFF, 0x0007C000};
const uint32_t n64_kb_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
const uint16_t n64_kb_scancode[KBM_MAX] = {
    0x070D, 0x0705, 0x070C, 0x0105, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0502, 0x0504, 0x0503, 0x0402, 0x0000, 0x0000, 0x0000, 0x0000,
    0x010C, 0x0107, 0x0106, 0x0706, 0x080A, 0x040D, 0x070F, 0x050D,
    0x0000, 0x080D, 0x0711, 0x0000, 0x0000, 0x080C, 0x010E, 0x0206,

    0x0807, 0x0805, 0x0707, 0x0708, 0x0408, 0x0709, 0x0309, 0x0308,
    0x0809, 0x0808, 0x0407, 0x0406, 0x0108, 0x0409, 0x0806, 0x0109,
    0x050C, 0x0505, 0x0506, 0x0507, 0x0508, 0x0509, 0x0609, 0x0608,
    0x0607, 0x0606, 0x060D, 0x010D, 0x0605, 0x060C, 0x040C, 0x0604,

    0x0410, 0x0307, 0x0306, 0x0405, 0x0209, 0x0208, 0x0207, 0x050F,
    0x010B, 0x010A, 0x080B, 0x070A, 0x070B, 0x020A, 0x020B, 0x030A,
    0x030B, 0x040A, 0x0302, 0x060B, 0x050B, 0x0802, 0x0702, 0x0000,
    0x0000, 0x0000, 0x0511, 0x0602, 0x0000, 0x050A, 0x0000, 0x0000,

    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0810, 0x0610,
    0x060E, 0x020E, 0x0210,
};

void n64_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
        {
            memset(wired_data->output, 0, sizeof(struct n64_kb_map));
            break;
        }
        default:
        {
            struct n64_map *map = (struct n64_map *)wired_data->output;

            map->buttons = 0x0000;
            for (uint32_t i = 0; i < N64_AXES_MAX; i++) {
                map->axes[n64_axes_idx[i]] = n64_axes_meta[i].neutral;
            }
            break;
        }
    }
}

void n64_meta_init(int32_t dev_mode, struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*4);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < N64_AXES_MAX; j++) {
            switch (dev_mode) {
                case DEV_KB:
                    ctrl_data[i].mask = n64_kb_mask;
                    ctrl_data[i].desc = n64_kb_desc;
                    break;
                case DEV_MOUSE:
                    ctrl_data[i].mask = n64_mouse_mask;
                    ctrl_data[i].desc = n64_mouse_desc;
                    ctrl_data[i].axes[j + 2].meta = &n64_mouse_axes_meta[j];
                    break;
                case DEV_PAD:
                default:
                    ctrl_data[i].mask = n64_mask;
                    ctrl_data[i].desc = n64_desc;
                    ctrl_data[i].axes[j].meta = &n64_axes_meta[j];
                    break;
            }
        }
    }
}

static void n64_ctrl_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct n64_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & generic_btns_mask[i]) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= n64_btns_mask[i];
                map_mask &= ~n64_btns_mask[i];
            }
            else if (map_mask & n64_btns_mask[i]) {
                map_tmp.buttons &= ~n64_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 0; i < N64_AXES_MAX; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & n64_desc[0])) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                map_tmp.axes[n64_axes_idx[i]] = 127;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                map_tmp.axes[n64_axes_idx[i]] = -128;
            }
            else {
                map_tmp.axes[n64_axes_idx[i]] = (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

static void n64_mouse_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct n64_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & generic_btns_mask[i]) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= n64_mouse_btns_mask[i];
                map_mask &= ~n64_mouse_btns_mask[i];
            }
            else if (map_mask & n64_mouse_btns_mask[i]) {
                map_tmp.buttons &= ~n64_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 0; i < N64_AXES_MAX; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i + 2) & n64_mouse_desc[0])) {
            int32_t tmp_val = ctrl_data->axes[i + 2].value + (int8_t)map_tmp.axes[n64_axes_idx[i]];
            if (tmp_val > ctrl_data->axes[i + 2].meta->size_max) {
                map_tmp.axes[n64_axes_idx[i]] = 127;
            }
            else if (tmp_val < ctrl_data->axes[i + 2].meta->size_min) {
                map_tmp.axes[n64_axes_idx[i]] = -128;
            }
            else {
                map_tmp.axes[n64_axes_idx[i]] += (uint8_t)(ctrl_data->axes[i + 2].value + ctrl_data->axes[i + 2].meta->neutral);
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

static void n64_kb_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct n64_kb_map map_tmp = {0};
    uint32_t code_idx = 0;

    for (uint32_t i = 0; i < KBM_MAX && code_idx < ARRAY_SIZE(map_tmp.key_codes); i++) {
        if (ctrl_data->map_mask[i / 32] & BIT(i & 0x1F)) {
            if (ctrl_data->btns[i / 32].value & BIT(i & 0x1F)) {
                if (n64_kb_scancode[i]) {
                    map_tmp.key_codes[code_idx] = n64_kb_scancode[i];
                    code_idx++;
                }
            }
        }
    }

    if (ctrl_data->map_mask[2] & BIT(KB_HOME & 0x1F)) {
        if (ctrl_data->btns[2].value & BIT(KB_HOME & 0x1F)) {
            map_tmp.bitfield = 0x01;
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

void n64_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
            n64_kb_from_generic(ctrl_data, wired_data);
            break;
        case DEV_MOUSE:
            n64_mouse_from_generic(ctrl_data, wired_data);
            break;
        case DEV_PAD:
        default:
            n64_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
}

void n64_fb_to_generic(int32_t dev_mode, uint8_t *raw_fb_data, uint32_t raw_fb_len, struct generic_fb *fb_data) {
    fb_data->wired_id = raw_fb_data[0];
    fb_data->state = raw_fb_data[1];
    fb_data->cycles = 0;
    fb_data->start = 0;
}
