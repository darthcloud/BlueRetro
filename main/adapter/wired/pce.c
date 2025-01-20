/*
 * Copyright (c) 2021-2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "adapter/config.h"
#include "adapter/wired/wired.h"
#include "system/manager.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "tests/cmds.h"
#include "bluetooth/mon.h"
#include "pce.h"
#include "driver/gpio.h"

#define P1_SEL_PIN 33
#define P1_OE_PIN 26
#define P1_U_I_PIN 3
#define P1_R_II_PIN 5
#define P1_D_SL_PIN 18
#define P1_L_RN_PIN 23

#define P1_SEL_MASK (1 << (P1_SEL_PIN - 32))
#define P1_OE_MASK (1 << P1_OE_PIN)
#define P1_U_I_MASK (1 << P1_U_I_PIN)
#define P1_R_II_MASK (1 << P1_R_II_PIN)
#define P1_D_SL_MASK (1 << P1_D_SL_PIN)
#define P1_L_RN_MASK (1 << P1_L_RN_PIN)

#define P1_III_MASK P1_U_I_MASK
#define P1_IV_MASK P1_R_II_MASK
#define P1_V_MASK P1_D_SL_MASK
#define P1_VI_MASK P1_L_RN_MASK

#define P1_OUT0_MASK (BIT(P1_U_I_PIN) | BIT(P1_R_II_PIN) | BIT(P1_D_SL_PIN) | BIT(P1_L_RN_PIN))

#define PCE_OUT_DISABLE ~P1_OUT0_MASK

enum {
    PCE_B = 0,
    PCE_C,
    PCE_A,
    PCE_START,
    PCE_LD_UP,
    PCE_LD_DOWN,
    PCE_LD_LEFT,
    PCE_LD_RIGHT,
    PCE_Z = 12,
    PCE_Y,
    PCE_X,
    PCE_MODE,
};

static DRAM_ATTR const uint8_t pce_mouse_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       0,       1,       0,     1
};

static DRAM_ATTR const struct ctrl_meta pce_mouse_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128},
};

struct pce_map {
    uint32_t buttons[5];
} __packed;

struct pce_mouse_map {
    int32_t raw_axes[2];
    uint32_t buttons;
    uint8_t relative[2];
} __packed;

struct pce_kb_map {
    uint32_t scancodes[18][2];
} __packed;

struct pce_kb_key_to_matrix {
    uint8_t idx;
    uint8_t nibble_idx;
    uint32_t mask;
};

static const uint32_t pce_mouse_mask[4] = {0x190100F0, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t pce_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t pce_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    P1_D_SL_MASK, 0, 0, 0,
    0, 0, 0, 0,
    P1_U_I_MASK, 0, 0, P1_L_RN_MASK,
    P1_R_II_MASK, 0, 0, 0,
};

static const uint32_t pce_mask[4] = {0x00750F00, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t pce_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t pce_btns_mask[][32] = {
    /* URDL */
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        P1_L_RN_MASK, P1_R_II_MASK, P1_D_SL_MASK, P1_U_I_MASK,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* 12SR */
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        P1_R_II_MASK, 0, P1_U_I_MASK, 0,
        P1_L_RN_MASK, P1_D_SL_MASK, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* 3456 */
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
};

static const uint32_t pce_6btns_mask[4] = {0x337F0F00, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t pce_6btns_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t pce_6btns_btns_mask[][32] = {
    /* URDL */
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        P1_L_RN_MASK, P1_R_II_MASK, P1_D_SL_MASK, P1_U_I_MASK,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* 12SR */
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, P1_U_I_MASK, P1_R_II_MASK, 0,
        P1_L_RN_MASK, P1_D_SL_MASK, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* 3456 */
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        P1_III_MASK, 0, 0, P1_V_MASK,
        0, 0, 0, 0,
        P1_IV_MASK, P1_IV_MASK, 0, 0,
        P1_VI_MASK, P1_VI_MASK, 0, 0,
    },
};

static const uint32_t pce_kb_mask[4] = {0xE6FF0F0F, 0xFFFFFFFF, 0xFFFFFFFF, 0x0007FFFF | BR_COMBO_MASK};
static const uint32_t pce_kb_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const struct pce_kb_key_to_matrix pce_kb_key_to_matrix[KBM_MAX] = {
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    {3, 0, P1_R_II_MASK}, {3, 1, P1_U_I_MASK}, {5, 0, P1_L_RN_MASK}, {14, 1, P1_D_SL_MASK}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    {11, 0, P1_D_SL_MASK}, {9, 0, P1_D_SL_MASK}, {11, 0, P1_R_II_MASK}, {9, 0, P1_R_II_MASK}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    {5, 0, P1_R_II_MASK}, {5, 0, P1_D_SL_MASK}, {3, 1, P1_R_II_MASK}, {3, 1, P1_D_SL_MASK}, {15, 1, P1_D_SL_MASK}, {15, 0, P1_U_I_MASK}, {9, 1, P1_R_II_MASK}, {0, 0, 0},
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    {0, 0, 0}, {6, 0, P1_D_SL_MASK}, {14, 1, P1_U_I_MASK}, {0, 0, 0}, {0, 0, 0}, {6, 0, P1_U_I_MASK}, {15, 0, P1_D_SL_MASK}, {10, 1, P1_D_SL_MASK},

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    {3, 0, P1_D_SL_MASK}, {3, 0, P1_L_RN_MASK}, {12, 1, P1_D_SL_MASK}, {4, 0, P1_U_I_MASK}, {4, 0, P1_R_II_MASK}, {4, 0, P1_D_SL_MASK}, {4, 0, P1_L_RN_MASK}, {4, 1, P1_U_I_MASK},
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    {4, 1, P1_R_II_MASK}, {4, 1, P1_D_SL_MASK}, {12, 1, P1_U_I_MASK}, {5, 0, P1_U_I_MASK}, {5, 1, P1_U_I_MASK}, {5, 1, P1_R_II_MASK}, {5, 1, P1_D_SL_MASK}, {6, 0, P1_R_II_MASK},
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    {7, 0, P1_R_II_MASK}, {7, 0, P1_D_SL_MASK}, {7, 0, P1_L_RN_MASK}, {7, 1, P1_U_I_MASK}, {7, 1, P1_R_II_MASK}, {7, 1, P1_D_SL_MASK}, {16, 1, P1_D_SL_MASK}, {8, 0, P1_U_I_MASK},
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
    {8, 0, P1_R_II_MASK}, {7, 0, P1_U_I_MASK}, {13, 1, P1_R_II_MASK}, {11, 0, P1_U_I_MASK}, {16, 1, P1_R_II_MASK}, {6, 1, P1_D_SL_MASK}, {6, 0, P1_L_RN_MASK}, {6, 1, P1_R_II_MASK},

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    {12, 1, P1_R_II_MASK}, {8, 0, P1_L_RN_MASK}, {8, 0, P1_D_SL_MASK}, {6, 1, P1_U_I_MASK}, {8, 1, P1_U_I_MASK}, {8, 1, P1_R_II_MASK}, {8, 1, P1_D_SL_MASK}, {15, 1, P1_U_I_MASK},
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    {10, 0, P1_R_II_MASK}, {10, 0, P1_D_SL_MASK}, {10, 0, P1_L_RN_MASK}, {10, 1, P1_U_I_MASK}, {10, 1, P1_R_II_MASK}, {13, 0, P1_U_I_MASK}, {13, 0, P1_R_II_MASK}, {13, 0, P1_D_SL_MASK},
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    {13, 0, P1_L_RN_MASK}, {13, 1, P1_U_I_MASK}, {2, 1, P1_R_II_MASK}, {2, 1, P1_U_I_MASK}, {11, 0, P1_L_RN_MASK}, {14, 0, P1_R_II_MASK}, {14, 0, P1_U_I_MASK}, {13, 1, P1_D_SL_MASK},
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    {11, 1, P1_U_I_MASK}, {12, 0, P1_R_II_MASK}, {15, 1, P1_R_II_MASK}, {10, 0, P1_U_I_MASK}, {12, 0, P1_U_I_MASK}, {9, 0, P1_U_I_MASK}, {11, 1, P1_D_SL_MASK}, {2, 0, P1_D_SL_MASK},

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    {11, 1, P1_R_II_MASK}, {2, 0, P1_L_RN_MASK}, {15, 0, P1_R_II_MASK}, {1, 0, P1_R_II_MASK}, {1, 0, P1_D_SL_MASK}, {1, 0, P1_L_RN_MASK}, {1, 1, P1_U_I_MASK}, {1, 1, P1_R_II_MASK},
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    {1, 1, P1_D_SL_MASK}, {14, 1, P1_R_II_MASK}, {2, 0, P1_U_I_MASK}, {2, 0, P1_R_II_MASK}, {1, 0, P1_U_I_MASK}, {2, 1, P1_D_SL_MASK}, {9, 1, P1_U_I_MASK}, {14, 0, P1_L_RN_MASK},
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    {15, 0, P1_L_RN_MASK}, {3, 0, P1_U_I_MASK}, {14, 0, P1_D_SL_MASK},
};

static void pce_ctrl_special_action(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
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

static void pce_mouse_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct pce_mouse_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~pce_mouse_btns_mask[i];
            }
            else {
                map_tmp.buttons |= pce_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 2; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & pce_mouse_desc[0])) {
            if (ctrl_data->axes[i].relative) {
                map_tmp.relative[pce_mouse_axes_idx[i]] = 1;
                atomic_add(&raw_axes[pce_mouse_axes_idx[i]], ctrl_data->axes[i].value);
            }
            else {
                map_tmp.relative[pce_mouse_axes_idx[i]] = 0;
                raw_axes[pce_mouse_axes_idx[i]] = ctrl_data->axes[i].value;
            }
        }
    }

    memcpy(wired_data->output + 8, (uint8_t *)&map_tmp + 8, sizeof(map_tmp) - 8);
}

static void pce_kb_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct pce_kb_map map_tmp;

    memset(&map_tmp, 0xFF, sizeof(map_tmp));

    for (uint32_t i = 0; i < KBM_MAX; i++) {
        if (ctrl_data->map_mask[i / 32] & BIT(i & 0x1F)) {
            if (ctrl_data->btns[i / 32].value & BIT(i & 0x1F)) {
                uint8_t byte = pce_kb_key_to_matrix[i].idx;
                uint8_t nibble = pce_kb_key_to_matrix[i].nibble_idx;

                map_tmp.scancodes[byte][nibble] &= ~pce_kb_key_to_matrix[i].mask;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

static void pce_ctrl_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct pce_map map_tmp;
    uint32_t map_mask[3];
    const uint32_t (*btns_mask)[32] = (config.out_cfg[0].dev_mode == DEV_PAD_ALT) ? pce_6btns_btns_mask : pce_btns_mask;

    memset(map_mask, 0xFF, sizeof(map_mask));
    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                for (uint32_t j = 0; j < 3; j++) {
                    map_tmp.buttons[j] &= ~btns_mask[j][i];
                    map_mask[j] &= ~btns_mask[j][i];
                }
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else {
                for (uint32_t j = 0; j < 3; j++) {
                    if (map_mask[j] & btns_mask[j][i]) {
                        map_tmp.buttons[j] |= btns_mask[j][i];
                    }
                }
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    pce_ctrl_special_action(ctrl_data, wired_data);

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

    TESTS_CMDS_LOG("\"wired_output\": {\"btns\": [%ld, %ld, %ld, %ld, %ld]},\n",
        map_tmp.buttons[0], map_tmp.buttons[1], map_tmp.buttons[2], map_tmp.buttons[3], map_tmp.buttons[4]);
    BT_MON_LOG("\"wired_output\": {\"btns\": [%08lX, %08lX, %08lX, %08lX, %08lX]},\n",
        map_tmp.buttons[0], map_tmp.buttons[1], map_tmp.buttons[2], map_tmp.buttons[3], map_tmp.buttons[4]);
}

void IRAM_ATTR pce_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
        {
            struct pce_kb_map *map = (struct pce_kb_map *)wired_data->output;

            memset(wired_data->output, 0xFF, sizeof(struct pce_kb_map));
            map->scancodes[0][0] = P1_R_II_MASK;
            map->scancodes[0][1] = PCE_OUT_DISABLE;
            map->scancodes[17][0] = P1_R_II_MASK;
            map->scancodes[17][1] = PCE_OUT_DISABLE;
            break;
        }
        case DEV_MOUSE:
        {
            struct pce_mouse_map *map = (struct pce_mouse_map *)wired_data->output;

            map->buttons = 0xFFFDFFFD;
            for (uint32_t i = 0; i < 2; i++) {
                map->raw_axes[i] = 0;
                map->relative[i] = 1;
            }
            break;
        }
        default:
        {
            struct pce_map *map = (struct pce_map *)wired_data->output;
            struct pce_map *map_mask = (struct pce_map *)wired_data->output_mask;

            for (uint32_t i = 0; i < 3; i++) {
                map->buttons[i] = 0xFFFDFFFD;
                map_mask->buttons[i] = 0x00000000;
            }
            break;
        }
    }
}

void pce_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[0].dev_mode) {
                case DEV_KB:
                    ctrl_data[i].mask = pce_kb_mask;
                    ctrl_data[i].desc = pce_kb_desc;
                    break;
                case DEV_MOUSE:
                    ctrl_data[i].mask = pce_mouse_mask;
                    ctrl_data[i].desc = pce_mouse_desc;
                    ctrl_data[i].axes[j].meta = &pce_mouse_axes_meta[j];
                    break;
                case DEV_PAD_ALT:
                    ctrl_data[i].mask = pce_6btns_mask;
                    ctrl_data[i].desc = pce_6btns_desc;
                    break;
                default:
                    ctrl_data[i].mask = pce_mask;
                    ctrl_data[i].desc = pce_desc;
            }
        }
    }
}

void pce_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
            pce_kb_from_generic(ctrl_data, wired_data);
            break;
        case DEV_MOUSE:
            pce_mouse_from_generic(ctrl_data, wired_data);
            break;
        default:
            pce_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
}

void IRAM_ATTR pce_gen_turbo_mask(struct wired_data *wired_data) {
    const uint32_t (*btns_mask)[32] = (config.out_cfg[0].dev_mode == DEV_PAD_ALT) ? pce_6btns_btns_mask : pce_btns_mask;
    struct pce_map *map_mask = (struct pce_map *)wired_data->output_mask;

    memset(map_mask, 0, sizeof(*map_mask));

    wired_gen_turbo_mask_btns32(wired_data, map_mask->buttons, btns_mask, 3);
}
