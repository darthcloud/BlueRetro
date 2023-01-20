/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "adapter/wired/wired.h"
#include "gc.h"

enum {
    GC_A,
    GC_B,
    GC_X,
    GC_Y,
    GC_START,
    GC_LD_LEFT = 8,
    GC_LD_RIGHT,
    GC_LD_DOWN,
    GC_LD_UP,
    GC_Z,
    GC_R,
    GC_L,
};

static DRAM_ATTR const uint8_t gc_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       2,       3,       4,      5
};

static DRAM_ATTR const struct ctrl_meta gc_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x64, .abs_min = 0x64},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x64, .abs_min = 0x64},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x5C, .abs_min = 0x5C},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x5C, .abs_min = 0x5C},
    {.size_min = 0, .size_max = 255, .neutral = 0x20, .abs_max = 0xD0, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x20, .abs_max = 0xD0, .abs_min = 0x00},
};

struct gc_map {
    uint16_t buttons;
    uint8_t axes[6];
} __packed;

struct gc_kb_map {
    uint8_t salt;
    uint8_t bytes[3];
    uint8_t key_codes[3];
    uint8_t xor;
} __packed;

static const uint32_t gc_mask[4] = {0x771F0FFF, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t gc_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t gc_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(GC_LD_LEFT), BIT(GC_LD_RIGHT), BIT(GC_LD_DOWN), BIT(GC_LD_UP),
    0, 0, 0, 0,
    BIT(GC_B), BIT(GC_X), BIT(GC_A), BIT(GC_Y),
    BIT(GC_START), 0, 0, 0,
    0, BIT(GC_Z), BIT(GC_L), 0,
    0, BIT(GC_Z), BIT(GC_R), 0,
};

static const uint32_t gc_kb_mask[4] = {0xE6FF0F0F, 0xFFFFFFFF, 0x1FBFFFFF, 0x0003C000 | BR_COMBO_MASK};
static const uint32_t gc_kb_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const uint8_t gc_kb_scancode[KBM_MAX] = {
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    0x10, 0x13, 0x22, 0x26, 0x00, 0x00, 0x00, 0x00,
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    0x5C, 0x5F, 0x5D, 0x5E, 0x00, 0x00, 0x00, 0x00,
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    0x20, 0x21, 0x14, 0x15, 0x4c, 0x61, 0x58, 0x4f,
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    0x00, 0x29, 0x56, 0x00, 0x00, 0x27, 0x54, 0x59,

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    0x11, 0x12, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    0x1C, 0x1D, 0x1E, 0x1F, 0x23, 0x24, 0x25, 0x28,
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31,
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
    0x32, 0x33, 0x50, 0x51, 0x34, 0x35, 0x38, 0x3B,

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    0x3F, 0x39, 0x3A, 0x36, 0x3C, 0x3D, 0x3E, 0x53,
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    0x48, 0x49, 0x4A, 0x4B, 0x37, 0x0A, 0x00, 0x4D,
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    0x06, 0x08, 0x4E, 0x07, 0x09, 0x00, 0x00, 0x00,

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x57, 0x5B,
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    0x55, 0x5A, 0x00,
};

void IRAM_ATTR gc_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
        {
            memset(wired_data->output, 0, sizeof(struct gc_kb_map));
            break;
        }
        default:
        {
            struct gc_map *map = (struct gc_map *)wired_data->output;
            struct gc_map *map_mask = (struct gc_map *)wired_data->output_mask;

            map->buttons = 0x8020;
            map_mask->buttons = 0xFFFF;
            for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
                map->axes[gc_axes_idx[i]] = gc_axes_meta[i].neutral;
            }
            memset(map_mask->axes, 0x00, sizeof(map_mask->axes));
            break;
        }
    }
}

void gc_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*4);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_KB:
                    ctrl_data[i].mask = gc_kb_mask;
                    ctrl_data[i].desc = gc_kb_desc;
                    ctrl_data[i].axes[j].meta = NULL;
                    break;
                default:
                    ctrl_data[i].mask = gc_mask;
                    ctrl_data[i].desc = gc_desc;
                    ctrl_data[i].axes[j].meta = &gc_axes_meta[j];
                    break;
            }
        }
    }
}

static void gc_ctrl_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct gc_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & generic_btns_mask[i]) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= gc_btns_mask[i];
                map_mask &= ~gc_btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else if (map_mask & gc_btns_mask[i]) {
                map_tmp.buttons &= ~gc_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & gc_desc[0])) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                map_tmp.axes[gc_axes_idx[i]] = 255;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                map_tmp.axes[gc_axes_idx[i]] = 0;
            }
            else {
                map_tmp.axes[gc_axes_idx[i]] = (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
        wired_data->cnt_mask[axis_to_btn_id(i)] = ctrl_data->axes[i].cnt_mask;
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
    printf("{\"log_type\": \"wired_output\", \"axes\": [%d, %d, %d, %d, %d, %d], \"btns\": %d}\n",
        map_tmp.axes[gc_axes_idx[0]], map_tmp.axes[gc_axes_idx[1]], map_tmp.axes[gc_axes_idx[2]],
        map_tmp.axes[gc_axes_idx[3]], map_tmp.axes[gc_axes_idx[4]], map_tmp.axes[gc_axes_idx[5]], map_tmp.buttons);
#endif
}

static void gc_kb_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct gc_kb_map map_tmp = {0};
    uint32_t code_idx = 0;

    for (uint32_t i = 0; i < KBM_MAX && code_idx < ARRAY_SIZE(map_tmp.key_codes); i++) {
        if (ctrl_data->map_mask[i / 32] & BIT(i & 0x1F)) {
            if (ctrl_data->btns[i / 32].value & BIT(i & 0x1F)) {
                if (gc_kb_scancode[i]) {
                    map_tmp.key_codes[code_idx++] = gc_kb_scancode[i];
                }
            }
        }
    }
    map_tmp.salt = (wired_data->output[0] + 1) & 0xF;

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

void gc_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
            gc_kb_from_generic(ctrl_data, wired_data);
            break;
        case DEV_PAD:
        default:
            gc_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
}

void gc_fb_to_generic(int32_t dev_mode, struct raw_fb *raw_fb_data, struct generic_fb *fb_data) {
    fb_data->wired_id = raw_fb_data->header.wired_id;
    fb_data->type = raw_fb_data->header.type;
    fb_data->state = raw_fb_data->data[0];
    fb_data->cycles = 0;
    fb_data->start = 0;
}

void IRAM_ATTR gc_gen_turbo_mask(struct wired_data *wired_data) {
    struct gc_map *map_mask = (struct gc_map *)wired_data->output_mask;

    map_mask->buttons = 0xFFFF;
    memset(map_mask->axes, 0x00, sizeof(map_mask->axes));

    wired_gen_turbo_mask_btns16_pos(wired_data, &map_mask->buttons, gc_btns_mask);
    wired_gen_turbo_mask_axes8(wired_data, map_mask->axes, ADAPTER_MAX_AXES, gc_axes_idx, gc_axes_meta);
}
