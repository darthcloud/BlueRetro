/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "adapter/wired/wired.h"
#include "npiso.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#define VTAP_PAL_PIN 16
#define VTAP_MODE_PIN 27

#define P2_D1_PIN 25
#define P2_D2_PIN 3
#define P2_D3_PIN 16
#define P2_D4_PIN 27

#define P2_D1_MASK (1U << P2_D1_PIN)
#define P2_D2_MASK (1U << P2_D2_PIN)
#define P2_D3_MASK (1U << P2_D3_PIN)
#define P2_D4_MASK (1U << P2_D4_PIN)

enum {
    NPISO_LD_RIGHT = 0,
    NPISO_LD_LEFT,
    NPISO_LD_DOWN,
    NPISO_LD_UP,
    NPISO_START,
    NPISO_SELECT,
    NPISO_Y, NPISO_VB_RD_LEFT = NPISO_Y,
    NPISO_B, NPISO_VB_RD_DOWN = NPISO_B,
    NPISO_VB_A = 10,
    NPISO_VB_B,
    NPISO_R,
    NPISO_L,
    NPISO_X, NPISO_VB_RD_UP = NPISO_X,
    NPISO_A, NPISO_VB_RD_RIGHT = NPISO_A,
};

static DRAM_ATTR const uint8_t npiso_mouse_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    1,       0,       1,       0,       1,     0
};

static DRAM_ATTR const struct ctrl_meta npiso_mouse_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x7F, .abs_min = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x7F, .abs_min = 0x80, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x7F, .abs_min = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x7F, .abs_min = 0x80, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x7F, .abs_min = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x7F, .abs_min = 0x80},
};

static DRAM_ATTR const struct ctrl_meta npiso_trackball_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -8, .size_max = 7, .neutral = 0x00, .abs_max = 0x7, .abs_min = 0x8},
    {.size_min = -8, .size_max = 7, .neutral = 0x00, .abs_max = 0x7, .abs_min = 0x8, .polarity = 1},
    {.size_min = -8, .size_max = 7, .neutral = 0x00, .abs_max = 0x7, .abs_min = 0x8},
    {.size_min = -8, .size_max = 7, .neutral = 0x00, .abs_max = 0x7, .abs_min = 0x8, .polarity = 1},
    {.size_min = -8, .size_max = 7, .neutral = 0x00, .abs_max = 0x7, .abs_min = 0x8},
    {.size_min = -8, .size_max = 7, .neutral = 0x00, .abs_max = 0x7, .abs_min = 0x8},
};

struct npiso_map {
    uint16_t buttons;
} __packed;

struct npiso_mouse_map {
    uint8_t id;
    uint8_t buttons;
    uint8_t relative[2];
    int32_t raw_axes[2];
} __packed;

struct npiso_trackball_map {
    uint8_t buttons;
    uint8_t flags;
    uint8_t relative[2];
    int32_t raw_axes[2];
} __packed;

struct npiso_kb_map {
    uint32_t scancodes[9][2];
} __packed;

struct npiso_kb_key_to_matrix {
    uint8_t column_idx;
    uint8_t row_nibble_idx;
    uint32_t row_mask;
};

static const uint32_t npiso_mask[4] = {0x333F0F00, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t npiso_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t npiso_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(NPISO_LD_LEFT), BIT(NPISO_LD_RIGHT), BIT(NPISO_LD_DOWN), BIT(NPISO_LD_UP),
    0, 0, 0, 0,
    BIT(NPISO_Y), BIT(NPISO_A), BIT(NPISO_B), BIT(NPISO_X),
    BIT(NPISO_START), BIT(NPISO_SELECT), 0, 0,
    BIT(NPISO_L), BIT(NPISO_L), 0, 0,
    BIT(NPISO_R), BIT(NPISO_R), 0, 0,
};

static const uint32_t npiso_vb_mask[4] = {0xBBF50FF0, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t npiso_vb_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t npiso_vb_btns_mask[32] = {
    0, 0, 0, 0,
    BIT(NPISO_VB_RD_LEFT), BIT(NPISO_VB_RD_RIGHT), BIT(NPISO_VB_RD_DOWN), BIT(NPISO_VB_RD_UP),
    BIT(NPISO_LD_LEFT), BIT(NPISO_LD_RIGHT), BIT(NPISO_LD_DOWN), BIT(NPISO_LD_UP),
    0, 0, 0, 0,
    BIT(NPISO_VB_B), 0, BIT(NPISO_VB_A), 0,
    BIT(NPISO_START), BIT(NPISO_SELECT), BIT(NPISO_START), BIT(NPISO_SELECT),
    BIT(NPISO_L), BIT(NPISO_L), 0, 0,
    BIT(NPISO_R), BIT(NPISO_R), 0, 0,
};

static const uint32_t npiso_mouse_mask[4] = {0x110000F0, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t npiso_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t npiso_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(NPISO_B), 0, 0, 0,
    BIT(NPISO_Y), 0, 0, 0,
};

static const uint32_t npiso_trackball_mask[4] = {0x1907C0F0, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t npiso_trackball_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t npiso_trackball_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, BIT(NPISO_LD_DOWN), BIT(NPISO_LD_UP),
    BIT(NPISO_LD_LEFT), BIT(NPISO_LD_RIGHT), 0, 0,
    0, 0, 0, 0,
    BIT(NPISO_Y), 0, 0, BIT(NPISO_START),
    BIT(NPISO_B), 0, 0, 0,
};

static const uint32_t npiso_kb_mask[4] = {0xE6FF0F0F, 0xFFFFFFFF, 0xFFFFFFFF, 0x0007FFFF | BR_COMBO_MASK};
static const uint32_t npiso_kb_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const struct npiso_kb_key_to_matrix npiso_kb_key_to_matrix[KBM_MAX] = {
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    {6, 0, P2_D4_MASK}, {5, 0, P2_D4_MASK}, {6, 0, P2_D3_MASK}, {6, 0, P2_D2_MASK}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    {8, 0, P2_D4_MASK}, {8, 0, P2_D3_MASK}, {8, 1, P2_D1_MASK}, {8, 0, P2_D2_MASK}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    {7, 0, P2_D3_MASK}, {5, 0, P2_D3_MASK}, {6, 1, P2_D3_MASK}, {5, 1, P2_D1_MASK}, {7, 0, P2_D2_MASK}, {0, 0, P2_D2_MASK}, {0, 0, 0}, {0, 1, P2_D3_MASK},
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    {0, 0, 0}, {6, 1, P2_D2_MASK}, {7, 0, P2_D4_MASK}, {0, 0, 0}, {0, 0, 0}, {6, 1, P2_D1_MASK}, {7, 1, P2_D1_MASK}, {8, 1, P2_D2_MASK},

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    {4, 1, P2_D1_MASK}, {5, 1, P2_D2_MASK}, {4, 0, P2_D3_MASK}, {4, 0, P2_D4_MASK}, {3, 0, P2_D2_MASK}, {3, 0, P2_D4_MASK}, {2, 0, P2_D4_MASK}, {2, 0, P2_D3_MASK},
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    {3, 1, P2_D1_MASK}, {3, 1, P2_D2_MASK}, {2, 0, P2_D2_MASK}, {2, 1, P2_D3_MASK}, {5, 0, P2_D2_MASK}, {3, 0, P2_D3_MASK}, {4, 1, P2_D2_MASK}, {4, 0, P2_D2_MASK},
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    {7, 1, P2_D3_MASK}, {7, 1, P2_D4_MASK}, {6, 1, P2_D4_MASK}, {5, 1, P2_D4_MASK}, {5, 1, P2_D3_MASK}, {4, 1, P2_D4_MASK}, {4, 1, P2_D3_MASK}, {3, 1, P2_D4_MASK},
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
    {3, 1, P2_D3_MASK}, {2, 1, P2_D4_MASK}, {0, 1, P2_D4_MASK}, {0, 0, 0}, {1, 1, P2_D4_MASK}, {1, 1, P2_D3_MASK}, {0, 0, P2_D3_MASK}, {0, 0, P2_D4_MASK},

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    {1, 1, P2_D1_MASK}, {1, 0, P2_D3_MASK}, {1, 0, P2_D4_MASK}, {0, 1, P2_D3_MASK}, {2, 1, P2_D2_MASK}, {2, 1, P2_D1_MASK}, {1, 1, P2_D2_MASK}, {0, 1, P2_D1_MASK},
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    {7, 0, P2_D1_MASK}, {6, 0, P2_D1_MASK}, {5, 0, P2_D1_MASK}, {4, 0, P2_D1_MASK}, {3, 0, P2_D1_MASK}, {2, 0, P2_D1_MASK}, {1, 0, P2_D1_MASK}, {0, 0, P2_D1_MASK},
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    {0, 0, P2_D1_MASK | P2_D2_MASK | P2_D3_MASK | P2_D4_MASK}, {0, 1, P2_D1_MASK | P2_D2_MASK | P2_D3_MASK | P2_D4_MASK}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {8, 1, P2_D4_MASK},
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    {8, 0, P2_D1_MASK}, {0, 0, 0}, {8, 1, P2_D3_MASK}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {7, 1, P2_D3_MASK}, {0, 0, 0}, {0, 0, 0},
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    {0, 1, P2_D2_MASK}, {1, 0, P2_D2_MASK}, {0, 0, 0},
};

void IRAM_ATTR npiso_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
        {
            memset(wired_data->output, 0x00, sizeof(struct npiso_kb_map));
            break;
        }
        case DEV_MOUSE:
        {
            if (wired_adapter.system_id == SNES) {
                struct npiso_mouse_map *map = (struct npiso_mouse_map *)wired_data->output;

                map->id = 0xFF;
                map->buttons = 0xFE;
                map->relative[0] = 1;
                map->relative[1] = 1;
                map->raw_axes[0] = 0;
                map->raw_axes[1] = 0;
            }
            else {
                struct npiso_trackball_map *map = (struct npiso_trackball_map *)wired_data->output;

                map->buttons = 0xFF;
                map->flags = 0x2F; // Fixed switch to L & Lo
                map->relative[0] = 1;
                map->relative[1] = 1;
                map->raw_axes[0] = 0;
                map->raw_axes[1] = 0;
            }
            break;
        }
        default:
        {
            struct npiso_map *map = (struct npiso_map *)wired_data->output;
            struct npiso_map *map_mask = (struct npiso_map *)wired_data->output_mask;

            if (wired_adapter.system_id == VBOY) {
                map->buttons = 0xFDFF;
            }
            else {
                map->buttons = 0xFFFF;
            }
            map_mask->buttons = 0x0000;
            break;
        }
    }
}

void npiso_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_KB:
                    ctrl_data[i].mask = npiso_kb_mask;
                    ctrl_data[i].desc = npiso_kb_desc;
                    break;
                case DEV_MOUSE:
                    if (wired_adapter.system_id == SNES) {
                        ctrl_data[i].mask = npiso_mouse_mask;
                        ctrl_data[i].desc = npiso_mouse_desc;
                        ctrl_data[i].axes[j].meta = &npiso_mouse_axes_meta[j];
                    }
                    else {
                        ctrl_data[i].mask = npiso_trackball_mask;
                        ctrl_data[i].desc = npiso_trackball_desc;
                        ctrl_data[i].axes[j].meta = &npiso_trackball_axes_meta[j];
                    }
                    break;
                default:
                    if (wired_adapter.system_id == VBOY) {
                        ctrl_data[i].mask = npiso_vb_mask;
                        ctrl_data[i].desc = npiso_vb_desc;
                    }
                    else {
                        ctrl_data[i].mask = npiso_mask;
                        ctrl_data[i].desc = npiso_desc;
                    }
                    break;
            }
        }
    }
}

static void npiso_vtap_gpio(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    /* Palette */
    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_LJ]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_LJ]) {
            GPIO.out_w1tc = BIT(VTAP_PAL_PIN);
        }
        else {
            GPIO.out_w1ts = BIT(VTAP_PAL_PIN);
        }
    }

    /* Mode */
    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_RJ]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_RJ]) {
            if (!atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2)) {
                atomic_set_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2);
            }
        }
        else {
            if (atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2)) {
                atomic_clear_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2);

                if (GPIO.out & BIT(VTAP_MODE_PIN)) {
                    GPIO.out_w1tc = BIT(VTAP_MODE_PIN);
                }
                else {
                    GPIO.out_w1ts = BIT(VTAP_MODE_PIN);
                }
            }
        }
    }
}

static void npiso_ctrl_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct npiso_map map_tmp;
    uint32_t map_mask = 0xFFFF;
    const uint32_t *btns_mask = (wired_adapter.system_id == VBOY) ? npiso_vb_btns_mask : npiso_btns_mask;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~btns_mask[i];
                map_mask &= ~btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else if (map_mask & btns_mask[i]) {
                map_tmp.buttons |= btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
    printf("{\"log_type\": \"wired_output\", \"btns\": %d}\n", map_tmp.buttons);
#endif
}

static void npiso_mouse_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct npiso_mouse_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 4);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~npiso_mouse_btns_mask[i];
            }
            else {
                map_tmp.buttons |= npiso_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 2; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & npiso_mouse_desc[0])) {
            if (ctrl_data->axes[i].relative) {
                map_tmp.relative[npiso_mouse_axes_idx[i]] = 1;
                atomic_add(&raw_axes[npiso_mouse_axes_idx[i]], ctrl_data->axes[i].value);
            }
            else {
                map_tmp.relative[npiso_mouse_axes_idx[i]] = 0;
                raw_axes[npiso_mouse_axes_idx[i]] = ctrl_data->axes[i].value;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 8);
}

static void npiso_trackball_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct npiso_trackball_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 4);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~npiso_trackball_btns_mask[i];
            }
            else {
                map_tmp.buttons |= npiso_trackball_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 2; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & npiso_trackball_desc[0])) {
            if (ctrl_data->axes[i].relative) {
                map_tmp.relative[npiso_mouse_axes_idx[i]] = 1;
                atomic_add(&raw_axes[npiso_mouse_axes_idx[i]], ctrl_data->axes[i].value);
            }
            else {
                map_tmp.relative[npiso_mouse_axes_idx[i]] = 0;
                raw_axes[npiso_mouse_axes_idx[i]] = ctrl_data->axes[i].value;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 8);
}

static void npiso_kb_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct npiso_kb_map map_tmp = {0};

    for (uint32_t i = 0; i < KBM_MAX; i++) {
        if (ctrl_data->map_mask[i / 32] & BIT(i & 0x1F)) {
            if (ctrl_data->btns[i / 32].value & BIT(i & 0x1F)) {
                uint8_t column = npiso_kb_key_to_matrix[i].column_idx;
                uint8_t row = npiso_kb_key_to_matrix[i].row_nibble_idx;

                map_tmp.scancodes[column][row] |= npiso_kb_key_to_matrix[i].row_mask;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

void npiso_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
            npiso_kb_from_generic(ctrl_data, wired_data);
            break;
        case DEV_MOUSE:
            if (wired_adapter.system_id == SNES) {
                npiso_mouse_from_generic(ctrl_data, wired_data);
            }
            else {
                npiso_trackball_from_generic(ctrl_data, wired_data);
            }
            break;
        default:
            npiso_ctrl_from_generic(ctrl_data, wired_data);
            if (wired_adapter.system_id == VBOY) {
                npiso_vtap_gpio(ctrl_data, wired_data);
            }
            break;
    }
}

void IRAM_ATTR npiso_gen_turbo_mask(struct wired_data *wired_data) {
    const uint32_t *btns_mask = (wired_adapter.system_id == VBOY) ? npiso_vb_btns_mask : npiso_btns_mask;
    struct npiso_map *map_mask = (struct npiso_map *)wired_data->output_mask;

    map_mask->buttons = 0x0000;

    wired_gen_turbo_mask_btns16_neg(wired_data, &map_mask->buttons, btns_mask);
}
