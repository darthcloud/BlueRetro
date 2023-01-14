/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "adapter/wired/wired.h"
#include "adapter/wireless/wireless.h"
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

static DRAM_ATTR const uint8_t n64_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       0,       1,       0,      1
};

static DRAM_ATTR const struct ctrl_meta n64_axes_meta[N64_AXES_MAX] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x54},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x54},
};

static DRAM_ATTR const struct ctrl_meta n64_mouse_axes_meta[N64_AXES_MAX] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x80},
};

struct n64_map {
    uint16_t buttons;
    uint8_t axes[2];
} __packed;

struct n64_mouse_map {
    uint16_t buttons;
    uint8_t relative[2];
    int32_t raw_axes[2];
} __packed;

struct n64_kb_map {
    uint16_t key_codes[3];
    uint8_t bitfield;
} __packed;

static const uint32_t n64_mask[4] = {0x33DF0FFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t n64_desc[4] = {0x0000000F, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t n64_btns_mask[32] = {
    0, 0, 0, 0,
    BIT(N64_C_LEFT), BIT(N64_C_RIGHT), BIT(N64_C_DOWN), BIT(N64_C_UP),
    BIT(N64_LD_LEFT), BIT(N64_LD_RIGHT), BIT(N64_LD_DOWN), BIT(N64_LD_UP),
    0, 0, 0, 0,
    BIT(N64_B), BIT(N64_C_DOWN), BIT(N64_A), BIT(N64_C_LEFT),
    BIT(N64_START), 0, 0, 0,
    BIT(N64_Z), BIT(N64_L), 0, 0,
    BIT(N64_Z), BIT(N64_R), 0, 0,
};

static const uint32_t n64_mouse_mask[4] = {0x110000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t n64_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t n64_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(N64_B), 0, 0, 0,
    BIT(N64_A), 0, 0, 0,
};

static const uint32_t n64_kb_mask[4] = {0xE6FF0F0F, 0xFFFFFFFF, 0x2D7FFFFF, 0x0007C000};
static const uint32_t n64_kb_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const uint16_t n64_kb_scancode[KBM_MAX] = {
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    0x070D, 0x0705, 0x070C, 0x0105, 0x0000, 0x0000, 0x0000, 0x0000,
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    0x0502, 0x0504, 0x0503, 0x0402, 0x0000, 0x0000, 0x0000, 0x0000,
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    0x010C, 0x0107, 0x0106, 0x0706, 0x080A, 0x040D, 0x070F, 0x050D,
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    0x0000, 0x080D, 0x0711, 0x0000, 0x0000, 0x080C, 0x010E, 0x0206,

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    0x0807, 0x0805, 0x0707, 0x0708, 0x0408, 0x0709, 0x0309, 0x0308,
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    0x0809, 0x0808, 0x0407, 0x0406, 0x0108, 0x0409, 0x0806, 0x0109,
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    0x050C, 0x0505, 0x0506, 0x0507, 0x0508, 0x0509, 0x0609, 0x0608,
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
    0x0607, 0x0606, 0x060D, 0x010D, 0x0605, 0x060C, 0x040C, 0x0604,

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    0x0410, 0x0307, 0x0306, 0x0405, 0x0209, 0x0208, 0x0207, 0x050F,
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    0x010B, 0x010A, 0x080B, 0x070A, 0x070B, 0x020A, 0x020B, 0x030A,
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    0x030B, 0x040A, 0x0302, 0x060B, 0x050B, 0x0802, 0x0702, 0x0000,
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    0x0000, 0x0000, 0x0511, 0x0602, 0x0000, 0x050A, 0x0000, 0x0000,

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0810, 0x0610,
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    0x060E, 0x020E, 0x0210,
};

void IRAM_ATTR n64_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
        {
            memset(wired_data->output, 0, sizeof(struct n64_kb_map));
            break;
        }
        case DEV_MOUSE:
        {
            struct n64_mouse_map *map = (struct n64_mouse_map *)wired_data->output;

            for (uint32_t i = 0; i < N64_AXES_MAX; i++) {
                map->raw_axes[i] = 0;
                map->relative[i] = 1;
            }
            map->buttons = 0x0000;
            break;
        }
        default:
        {
            struct n64_map *map = (struct n64_map *)wired_data->output;

            map->buttons = 0x0000;
            for (uint32_t i = 0; i < N64_AXES_MAX; i++) {
                map->axes[n64_axes_idx[i]] = n64_axes_meta[i].neutral;
            }
            memset(wired_data->output_mask, 0xFF, sizeof(struct n64_map));
            break;
        }
    }
}

void n64_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*4);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < N64_AXES_MAX; j++) {
            switch (config.out_cfg[i].dev_mode) {
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

static void n64_acc_toggle_fb(uint32_t wired_id, uint32_t duration_us) {
    struct bt_dev *device = NULL;
    struct bt_data *bt_data = NULL;

    bt_host_get_dev_from_out_idx(wired_id, &device);
    if (device) {
        bt_data = &bt_adapter.data[device->ids.id];
        if (bt_data) {
            struct generic_fb fb_data = {0};

            fb_data.wired_id = wired_id;
            fb_data.type = FB_TYPE_RUMBLE;
            fb_data.cycles = 0;
            fb_data.start = 0;
            fb_data.state = 1;
            adapter_fb_stop_timer_start(wired_id, duration_us);
            wireless_fb_from_generic(&fb_data, bt_data);
            bt_hid_feedback(device, bt_data->base.output);
        }
    }

}

static void n64_ctrl_special_action(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    /* Memory / Rumble toggle */
    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_MT]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_MT]) {
            if (!atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_set_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);
            }
        }
        else {
            if (atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_clear_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);

                /* Change config directly but do not update file */
                if (config.out_cfg[ctrl_data->index].acc_mode == ACC_MEM) {
                    config.out_cfg[ctrl_data->index].acc_mode = ACC_RUMBLE;
                    n64_acc_toggle_fb(ctrl_data->index, 250000);
                    printf("# %s: Set rumble pak\n", __FUNCTION__);
                }
                else {
                    config.out_cfg[ctrl_data->index].acc_mode = ACC_MEM;
                    n64_acc_toggle_fb(ctrl_data->index, 75000);
                    printf("# %s: Set ctrl pak\n", __FUNCTION__);
                }
            }
        }
    }

    /* Bank rotation */
    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_MQ]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_MQ]) {
            if (!atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2)) {
                atomic_set_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2);
            }
        }
        else {
            if (atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2)) {
                atomic_clear_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2);

                config.global_cfg.banksel = (config.global_cfg.banksel + 1) & 0x3;
                printf("# %s: Set ctrl pak bank to %d\n", __FUNCTION__, config.global_cfg.banksel);
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
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else if (map_mask & n64_btns_mask[i]) {
                map_tmp.buttons &= ~n64_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    n64_ctrl_special_action(ctrl_data, wired_data);

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
        wired_data->cnt_mask[axis_to_btn_id(i)] = ctrl_data->axes[i].cnt_mask;
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

static void n64_mouse_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct n64_mouse_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 4);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= n64_mouse_btns_mask[i];
            }
            else {
                map_tmp.buttons &= ~n64_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 2; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & n64_mouse_desc[0])) {
            if (ctrl_data->axes[i].relative) {
                map_tmp.relative[n64_axes_idx[i]] = 1;
                atomic_add(&raw_axes[n64_axes_idx[i]], ctrl_data->axes[i].value);
            }
            else {
                map_tmp.relative[n64_axes_idx[i]] = 0;
                raw_axes[n64_axes_idx[i]] = ctrl_data->axes[i].value;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 8);
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

    /* map_mask is in IRAM but somehow GCC emit an l8ui which crash the ESP32 */
    /* volatile var force it to use a l32 */
    volatile uint32_t mask = ctrl_data->map_mask[2];
    if (mask & BIT(KB_HOME & 0x1F)) {
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

void n64_fb_to_generic(int32_t dev_mode, struct raw_fb *raw_fb_data, struct generic_fb *fb_data) {
    fb_data->wired_id = raw_fb_data->header.wired_id;
    fb_data->type = raw_fb_data->header.type;
    fb_data->state = raw_fb_data->data[0];
    fb_data->cycles = 0;
    fb_data->start = 0;
}

void IRAM_ATTR n64_gen_turbo_mask(struct wired_data *wired_data) {
    struct n64_map *map_mask = (struct n64_map *)wired_data->output_mask;

    memset(map_mask, 0xFF, sizeof(*map_mask));

    wired_gen_turbo_mask_btns16_pos(wired_data, &map_mask->buttons, n64_btns_mask);
    wired_gen_turbo_mask_axes8(wired_data, map_mask->axes, N64_AXES_MAX, n64_axes_idx, n64_axes_meta);
}
