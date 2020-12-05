/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "../zephyr/types.h"
#include "../util.h"
#include "config.h"
#include "kb_monitor.h"
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
    union {
        uint8_t axes[4];
        struct {
            uint8_t flags;
            uint8_t scancode;
        };
    };
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

static const uint32_t saturn_kb_mask[4] = {0xE6FF0F0F, 0xFFFFFFFF, 0xFFFFFFFF, 0x0007FFFF};
static const uint32_t saturn_kb_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const uint8_t saturn_kb_scancode[KBM_MAX] = {
 /* Source: https://plutiedev.com/saturn-keyboard */
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    0x1C, 0x23, 0x1B, 0x1D, 0x00, 0x00, 0x00, 0x00,
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    0x86, 0x8D, 0x8A, 0x89, 0x00, 0x00, 0x00, 0x00,
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    0x15, 0x2D, 0x24, 0x2B, 0x76, 0x5A, 0x1F, 0x0E,
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    0x00, 0x1A, 0x14, 0x00, 0x00, 0x22, 0x12, 0x29,

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    0x32, 0x21, 0x34, 0x33, 0x43, 0x3B, 0x42, 0x4B,
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    0x3A, 0x31, 0x44, 0x4D, 0x2C, 0x3C, 0x2A, 0x35,
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    0x16, 0x1E, 0x26, 0x25, 0x2E, 0x36, 0x3D, 0x3E,
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
    0x46, 0x45, 0x66, 0x0D, 0x4E, 0x55, 0x54, 0x5B,

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    0x5D, 0x4C, 0x51, 0x00, 0x41, 0x49, 0x4A, 0x58,
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    0x05, 0x06, 0x04, 0x0C, 0x03, 0x0B, 0x83, 0x0A,
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    0x01, 0x09, 0x78, 0x07, 0x84, 0x7E, 0x82, 0x81,
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    0x87, 0x8B, 0x85, 0x88, 0x8C, 0x77, 0x80, 0x7C,

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    0x7B, 0x79, 0x19, 0x69, 0x72, 0x7A, 0x6B, 0x73,
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    0x74, 0x6C, 0x75, 0x7D, 0x70, 0x71, 0x11, 0x18,
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    0x59, 0x17, 0x00,
};

void saturn_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
        {
            struct saturn_map *map = (struct saturn_map *)wired_data->output;

            map->buttons = 0xFFFF;
            map->flags = 0x06;
            map->scancode = 0x00;
            break;
        }
        case DEV_MOUSE:
        {
            break;
        }
        default:
        {
            struct saturn_map *map = (struct saturn_map *)wired_data->output;

            map->buttons = 0xFFFF;
            for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
                if (i == 2 || i == 3) {
                    continue;
                }
                map->axes[saturn_axes_idx[i]] = saturn_axes_meta[i].neutral;
            }
            break;
        }
    }
}

void saturn_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_KB:
                    ctrl_data[i].mask = saturn_kb_mask;
                    ctrl_data[i].desc = saturn_kb_desc;
                    goto exit_axes_loop;
                case DEV_MOUSE:
                    //break;
                default:
                    ctrl_data[i].mask = saturn_mask;
                    ctrl_data[i].desc = saturn_desc;
                    ctrl_data[i].axes[j].meta = &saturn_axes_meta[j];
                    break;
            }
        }
exit_axes_loop:
        ;
    }
}

void saturn_ctrl_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
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

void saturn_mouse_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
}

void saturn_kb_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    uint16_t buttons = *(uint16_t *)wired_data->output;

    /* Use BlueRetro KB/Gamepad mapping here */
    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                buttons &= ~saturn_btns_mask[i];
            }
            else {
                buttons |= saturn_btns_mask[i];
            }
        }
    }

    *(uint16_t *)wired_data->output = buttons;

    kbmon_update(ctrl_data->index, ctrl_data);
}

void saturn_kb_id_to_scancode(uint8_t dev_id, uint8_t type, uint8_t id) {
    uint8_t kb_buf[2] = {0};
    switch (type) {
        case KBMON_TYPE_MAKE:
            kb_buf[0] = 0x08;
            break;
        case KBMON_TYPE_BREAK:
            kb_buf[0] = 0x01;
            break;
    }
    if (id < KBM_MAX) {
        kb_buf[1] = saturn_kb_scancode[id];
    }
    kbmon_set_code(dev_id, kb_buf, sizeof(kb_buf));
}

void saturn_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
            saturn_kb_from_generic(ctrl_data, wired_data);
            break;
        case DEV_MOUSE:
            saturn_mouse_from_generic(ctrl_data, wired_data);
            break;
        default:
            saturn_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
}
