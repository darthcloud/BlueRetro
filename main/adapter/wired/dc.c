/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "adapter/wired/wired.h"
#include "system/manager.h"
#include "dc.h"

enum {
    DC_Z = 0,
    DC_Y,
    DC_X,
    DC_D,
    DC_RD_UP,
    DC_RD_DOWN,
    DC_RD_LEFT,
    DC_RD_RIGHT,
    DC_C,
    DC_B,
    DC_A,
    DC_START,
    DC_LD_UP,
    DC_LD_DOWN,
    DC_LD_LEFT,
    DC_LD_RIGHT,
};

enum {
    DC_KB_LCTRL = 0,
    DC_KB_LSHIFT,
    DC_KB_LALT,
    DC_KB_LWIN,
    DC_KB_RCTRL,
    DC_KB_RSHIFT,
    DC_KB_RALT,
    DC_KB_RWIN,
};

static DRAM_ATTR const uint8_t dc_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    7,       6,       5,       4,       0,      1
};

static const uint8_t dc_kb_keycode_idx[] =
{
    1, 0, 7, 6, 5, 4
};

static DRAM_ATTR const struct ctrl_meta dc_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80, .polarity = 1},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
};

static DRAM_ATTR const struct ctrl_meta dc_mouse_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -512, .size_max = 511, .neutral = 0x200, .abs_max = 0x1FF, .abs_min = 0x200},
    {.size_min = -512, .size_max = 511, .neutral = 0x200, .abs_max = 0x1FF, .abs_min = 0x200},
    {.size_min = -512, .size_max = 511, .neutral = 0x200, .abs_max = 0x1FF, .abs_min = 0x200},
    {.size_min = -512, .size_max = 511, .neutral = 0x200, .abs_max = 0x1FF, .abs_min = 0x200, .polarity = 1},
    {.size_min = -512, .size_max = 511, .neutral = 0x200, .abs_max = 0x1FF, .abs_min = 0x200},
    {.size_min = -512, .size_max = 511, .neutral = 0x200, .abs_max = 0x1FF, .abs_min = 0x200},
};

struct dc_map {
    union {
        struct {
            uint16_t triggers;
            uint16_t buttons;
            uint32_t sticks;
        };
        uint8_t axes[8];
    };
} __packed;

struct dc_mouse_map {
    uint8_t reserved[3];
    uint8_t buttons;
    uint8_t relative[4];
    int32_t raw_axes[4];
} __packed;

struct dc_kb_map {
    union {
        struct {
            uint8_t reserved[3];
            uint8_t bitfield;
        };
        uint8_t key_codes[8];
    };
} __packed;

static const uint32_t dc_mask[4] = {0x337FFFFF, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t dc_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t dc_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(DC_LD_LEFT), BIT(DC_LD_RIGHT), BIT(DC_LD_DOWN), BIT(DC_LD_UP),
    BIT(DC_RD_LEFT), BIT(DC_RD_RIGHT), BIT(DC_RD_DOWN), BIT(DC_RD_UP),
    BIT(DC_X), BIT(DC_B), BIT(DC_A), BIT(DC_Y),
    BIT(DC_START), BIT(DC_D), 0, 0,
    0, BIT(DC_Z), 0, 0,
    0, BIT(DC_C), 0, 0,
};

static const uint32_t dc_mouse_mask[4] = {0x1901C0F0, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t dc_mouse_desc[4] = {0x0000C0F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t dc_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(DC_D), 0, 0, BIT(DC_Y),
    BIT(DC_X), 0, 0, 0,
};

static const uint32_t dc_kb_mask[4] = {0xE6FF0F0F, 0xFFFFFFFF, 0xFFFFFFFF, 0x0007FFFF | BR_COMBO_MASK};
static const uint32_t dc_kb_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const uint8_t dc_kb_scancode[KBM_MAX] = {
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    0x04, 0x07, 0x16, 0x1A, 0x00, 0x00, 0x00, 0x00,
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    0x50, 0x4F, 0x51, 0x52, 0x00, 0x00, 0x00, 0x00,
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    0x14, 0x15, 0x08, 0x09, 0x29, 0x28, 0x00, 0x35,
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    0x00, 0x1D, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x2C,

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    0x05, 0x06, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    0x10, 0x11, 0x12, 0x13, 0x17, 0x18, 0x19, 0x1C,
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
    0x26, 0x27, 0x2A, 0x2B, 0x2D, 0x2E, 0x30, 0x32,

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    0x87, 0x33, 0x34, 0x89, 0x36, 0x37, 0x38, 0x39,
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41,
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x53, 0x54, 0x55,

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D,
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x00, 0x00,
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    0x00, 0x00, 0x00,
};

static void dc_ctrl_special_action(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    /* Output config mode toggle GamePad/GamePadAlt */
    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_MT]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_MT]) {
            if (!atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_set_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);
            }
        }
        else {
            if (atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_clear_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);

                config.out_cfg[ctrl_data->index].dev_mode &= 0x01;
                config.out_cfg[ctrl_data->index].dev_mode ^= 0x01;
                sys_mgr_cmd(SYS_MGR_CMD_WIRED_RST);
            }
        }
    }
}

void IRAM_ATTR dc_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
        {
            memset(wired_data->output, 0x00, sizeof(struct dc_kb_map));
            break;
        }
        case DEV_MOUSE:
        {
            struct dc_mouse_map *map = (struct dc_mouse_map *)wired_data->output;

            for (uint32_t i = 0; i < ARRAY_SIZE(map->raw_axes); i++) {
                map->raw_axes[i] = 0;
                map->relative[i] = 1;
            }
            map->reserved[0] = 0x00;
            map->reserved[1] = 0x00;
            map->reserved[2] = 0x00;
            map->buttons = 0xFF;
            break;
        }
        default:
        {
            struct dc_map *map = (struct dc_map *)wired_data->output;
            struct dc_map *map_mask = (struct dc_map *)wired_data->output_mask;

            for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
                map->axes[dc_axes_idx[i]] = dc_axes_meta[i].neutral;
            }
            map->buttons = 0xFFFF;

            map_mask->triggers = 0xFFFF;
            map_mask->buttons = 0x0000;
            map_mask->sticks = 0x00000000;
            break;
        }
    }
}

void dc_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*4);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_KB:
                    ctrl_data[i].mask = dc_kb_mask;
                    ctrl_data[i].desc = dc_kb_desc;
                    goto exit_axes_loop;
                case DEV_MOUSE:
                    ctrl_data[i].mask = dc_mouse_mask;
                    ctrl_data[i].desc = dc_mouse_desc;
                    ctrl_data[i].axes[j].meta = &dc_mouse_axes_meta[j];
                    break;
                default:
                    ctrl_data[i].mask = dc_mask;
                    ctrl_data[i].desc = dc_desc;
                    ctrl_data[i].axes[j].meta = &dc_axes_meta[j];
                    break;
            }
        }
exit_axes_loop:
        ;
    }
}

static void dc_ctrl_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct dc_map map_tmp;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~dc_btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else {
                map_tmp.buttons |= dc_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    dc_ctrl_special_action(ctrl_data, wired_data);

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & dc_desc[0])) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                map_tmp.axes[dc_axes_idx[i]] = 255;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                map_tmp.axes[dc_axes_idx[i]] = 0;
            }
            else {
                map_tmp.axes[dc_axes_idx[i]] = (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
        wired_data->cnt_mask[axis_to_btn_id(i)] = ctrl_data->axes[i].cnt_mask;
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
    printf("{\"log_type\": \"wired_output\", \"axes\": [%d, %d, %d, %d, %d, %d], \"btns\": %d}\n",
        map_tmp.axes[dc_axes_idx[0]], map_tmp.axes[dc_axes_idx[1]], map_tmp.axes[dc_axes_idx[2]],
        map_tmp.axes[dc_axes_idx[3]], map_tmp.axes[dc_axes_idx[4]], map_tmp.axes[dc_axes_idx[5]], map_tmp.buttons);
#endif
}

static void dc_mouse_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct dc_mouse_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 8);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~dc_mouse_btns_mask[i];
            }
            else {
                map_tmp.buttons |= dc_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 0; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & dc_mouse_desc[0])) {
            if (ctrl_data->axes[i].relative) {
                map_tmp.relative[i] = 1;
                atomic_add(&raw_axes[i], ctrl_data->axes[i].value);
            }
            else {
                map_tmp.relative[i] = 0;
                raw_axes[i] = ctrl_data->axes[i].value;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 16);
}

static void dc_kb_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct dc_kb_map map_tmp = {0};
    uint32_t code_idx = 0;

    for (uint32_t i = 0; i < KBM_MAX && code_idx < ARRAY_SIZE(dc_kb_keycode_idx); i++) {
        if (ctrl_data->map_mask[i / 32] & BIT(i & 0x1F)) {
            if (ctrl_data->btns[i / 32].value & BIT(i & 0x1F)) {
                if (dc_kb_scancode[i]) {
                    map_tmp.key_codes[dc_kb_keycode_idx[code_idx++]] = dc_kb_scancode[i];
                }
            }
        }
    }

    if (ctrl_data->map_mask[0] & BIT(KB_LCTRL & 0x1F) && ctrl_data->btns[0].value & BIT(KB_LCTRL & 0x1F)) {
        map_tmp.bitfield |= BIT(DC_KB_LCTRL);
    }
    if (ctrl_data->map_mask[0] & BIT(KB_LSHIFT & 0x1F) && ctrl_data->btns[0].value & BIT(KB_LSHIFT & 0x1F)) {
        map_tmp.bitfield |= BIT(DC_KB_LSHIFT);
    }
    if (ctrl_data->map_mask[3] & BIT(KB_LALT & 0x1F) && ctrl_data->btns[3].value & BIT(KB_LALT & 0x1F)) {
        map_tmp.bitfield |= BIT(DC_KB_LALT);
    }
    if (ctrl_data->map_mask[0] & BIT(KB_LWIN & 0x1F) && ctrl_data->btns[0].value & BIT(KB_LWIN & 0x1F)) {
        map_tmp.bitfield |= BIT(DC_KB_LWIN);
    }
    if (ctrl_data->map_mask[3] & BIT(KB_RCTRL & 0x1F) && ctrl_data->btns[3].value & BIT(KB_RCTRL & 0x1F)) {
        map_tmp.bitfield |= BIT(DC_KB_RCTRL);
    }
    if (ctrl_data->map_mask[3] & BIT(KB_RSHIFT & 0x1F) && ctrl_data->btns[3].value & BIT(KB_RSHIFT & 0x1F)) {
        map_tmp.bitfield |= BIT(DC_KB_RSHIFT);
    }
    if (ctrl_data->map_mask[3] & BIT(KB_RALT & 0x1F) && ctrl_data->btns[3].value & BIT(KB_RALT & 0x1F)) {
        map_tmp.bitfield |= BIT(DC_KB_RALT);
    }
    if (ctrl_data->map_mask[3] & BIT(KB_RWIN & 0x1F) && ctrl_data->btns[3].value & BIT(KB_RWIN & 0x1F)) {
        map_tmp.bitfield |= BIT(DC_KB_RWIN);
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

void dc_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
            dc_kb_from_generic(ctrl_data, wired_data);
            break;
        case DEV_MOUSE:
            dc_mouse_from_generic(ctrl_data, wired_data);
            break;
        case DEV_PAD:
        default:
            dc_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
}

void dc_fb_to_generic(int32_t dev_mode, struct raw_fb *raw_fb_data, struct generic_fb *fb_data) {
    fb_data->wired_id = raw_fb_data->header.wired_id;
    fb_data->type = raw_fb_data->header.type;
    fb_data->cycles = 0;
    fb_data->start = 0;

    if (raw_fb_data->header.data_len == 0) {
        fb_data->state = 0;
        adapter_fb_stop_timer_stop(raw_fb_data->header.wired_id);
    }
    else {
        uint32_t dur_us = 1000 * ((*(uint16_t *)&raw_fb_data->data[0]) * 250 + 250);
        uint8_t freq = raw_fb_data->data[5];
        uint8_t mag0 = raw_fb_data->data[6] & 0x07;
        uint8_t mag1 = (raw_fb_data->data[6] >> 4) & 0x07;

        if (mag0 || mag1) {
            if (freq && ((raw_fb_data->data[6] & 0x88) || !(raw_fb_data->data[7] & 0x01))) {
                if (raw_fb_data->data[4]) {
                    dur_us = 1000000 * raw_fb_data->data[4] * MAX(mag0, mag1) / freq;
                }
                else {
                    dur_us = 1000000 / freq;
                }
            }
            fb_data->state = 1;
            adapter_fb_stop_timer_start(raw_fb_data->header.wired_id, dur_us);
        }
        else {
            fb_data->state = 0;
            adapter_fb_stop_timer_stop(raw_fb_data->header.wired_id);
        }
    }
}

void IRAM_ATTR dc_gen_turbo_mask(struct wired_data *wired_data) {
    struct dc_map *map_mask = (struct dc_map *)wired_data->output_mask;

    map_mask->triggers = 0xFFFF;
    map_mask->buttons = 0x0000;
    map_mask->sticks = 0x00000000;

    wired_gen_turbo_mask_btns16_neg(wired_data, &map_mask->buttons, dc_btns_mask);
    wired_gen_turbo_mask_axes8(wired_data, map_mask->axes, ADAPTER_MAX_AXES, dc_axes_idx, dc_axes_meta);
}
