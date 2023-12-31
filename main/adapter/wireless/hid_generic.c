/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "zephyr/types.h"
#include "zephyr/usb_hid.h"
#include "tools/util.h"
#include "hid_generic.h"
#include "adapter/mapping_quirks.h"
#include "adapter/hid_parser.h"

/* dinput buttons */
enum {
    HID_A = 0,
    HID_B,
    HID_C,
    HID_X,
    HID_Y,
    HID_Z,
    HID_LB,
    HID_RB,
    HID_L,
    HID_R,
    HID_SELECT,
    HID_START,
    HID_MENU,
    HID_LJ,
    HID_RJ,
};

struct hid_report_meta {
    int8_t hid_btn_idx;
    int8_t hid_axes_idx[ADAPTER_MAX_AXES];
    int8_t hid_hat_idx;
    uint8_t kb_bitfield;
    struct ctrl_meta hid_axes_meta[ADAPTER_MAX_AXES];
};

struct hid_reports_meta {
    struct hid_report_meta reports_meta[REPORT_MAX];
};

static struct hid_reports_meta devices_meta[BT_MAX_DEV] = {0};

struct generic_rumble {
    uint8_t state[88];
    uint8_t report_id;
    uint32_t report_size;
} __packed;

#define RUMBLE_ON_MULTIPLIER 1.0 // Range from 0.0 to 1.0

static const uint32_t hid_kb_bitfield_to_generic[8] = {
    KB_LCTRL,
    KB_LSHIFT,
    KB_LALT,
    KB_LWIN,
    KB_RCTRL,
    KB_RSHIFT,
    KB_RALT,
    KB_RWIN,
};

static const uint32_t hid_kb_key_to_generic[] = {
    0, 0, 0, 0, KB_A, KB_B, KB_C, KB_D,
    KB_E, KB_F, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L,
    KB_M, KB_N, KB_O, KB_P, KB_Q, KB_R, KB_S, KB_T,
    KB_U, KB_V, KB_W, KB_X, KB_Y, KB_Z, KB_1, KB_2,
    KB_3, KB_4, KB_5, KB_6, KB_7, KB_8, KB_9, KB_0,
    KB_ENTER, KB_ESC, KB_BACKSPACE, KB_TAB, KB_SPACE, KB_MINUS, KB_EQUAL, KB_LEFTBRACE,
    KB_RIGHTBRACE, KB_BACKSLASH, KB_HASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT,
    KB_SLASH, KB_CAPSLOCK, KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6,
    KB_F7, KB_F8, KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL,
    KB_PAUSE, KB_INSERT, KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_RIGHT,
    KB_LEFT, KB_DOWN, KB_UP, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI, KB_KP_MINUS, KB_KP_PLUS,
    KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5, KB_KP_6, KB_KP_7,
    KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT,
};

static const uint32_t hid_pad_default_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, BIT(HID_Z), 0, BIT(HID_C),
    BIT(HID_X), BIT(HID_B), BIT(HID_A), BIT(HID_Y),
    BIT(HID_START), BIT(HID_SELECT), BIT(HID_MENU), 0,
    BIT(HID_L), BIT(HID_LB), 0, BIT(HID_LJ),
    BIT(HID_R), BIT(HID_RB), 0, BIT(HID_RJ),
};

static void hid_kb_init(struct hid_report_meta *meta, struct hid_report *report, struct raw_src_mapping *map) {
    memset(meta->hid_axes_idx, -1, sizeof(meta->hid_axes_idx));
    meta->hid_btn_idx = -1;

    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        if (report->usages[i].usage > 0 && report->usages[i].usage < 0xE0) {
            meta->kb_bitfield = 1;
            break;
        }
    }
    for (uint32_t i = 0, key_idx = 0; i < report->usage_cnt; i++) {
        switch (report->usages[i].usage_page) {
            case USAGE_GEN_KEYBOARD:
                if (report->usages[i].usage >= 0xE0 && report->usages[i].usage <= 0xE7) {
                    map->mask[0] |= BIT(KB_LWIN & 0x1F) | BIT(KB_LCTRL & 0x1F) | BIT(KB_LSHIFT & 0x1F);
                    map->mask[3] |= BIT(KB_LALT & 0x1F) | BIT(KB_RCTRL & 0x1F) | BIT(KB_RSHIFT & 0x1F) | BIT(KB_RALT & 0x1F) | BIT(KB_RWIN & 0x1F);
                    meta->hid_btn_idx = i;
                }
                else if (key_idx < 6) {
                    meta->hid_axes_idx[key_idx] = i;
                    key_idx++;
                }
                break;
        }
    }
    map->mask[0] |= 0xE6FF0F0F;
    map->mask[1] |= 0xFFFFFFFF;
    map->mask[2] |= 0xFFFFFFFF;
    map->mask[3] |= 0x7FFFF;
}

static void hid_kb_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    struct hid_report_meta *meta = &devices_meta[bt_data->base.pids->id].reports_meta[KB];

    if (!atomic_test_bit(&bt_data->base.flags[KB], BT_INIT)) {
        hid_parser_load_report(bt_data, bt_data->base.report_id);
        hid_kb_init(meta, &bt_data->reports[KB], &bt_data->raw_src_mappings[KB]);
        atomic_set_bit(&bt_data->base.flags[KB], BT_INIT);
    }

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)bt_data->raw_src_mappings[KB].mask;
    ctrl_data->desc = (uint32_t *)bt_data->raw_src_mappings[KB].desc;

    if (meta->hid_btn_idx > -1) {
        uint32_t len = bt_data->reports[KB].usages[meta->hid_btn_idx].bit_size;
        uint32_t offset = bt_data->reports[KB].usages[meta->hid_btn_idx].bit_offset;
        uint32_t mask = (1ULL << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t buttons = ((*(uint32_t *)(bt_data->base.input + byte_offset)) >> bit_shift) & mask;

        for (uint8_t i = 0, mask = 1; mask; i++, mask <<= 1) {
            if (buttons & mask) {
                ctrl_data->btns[(hid_kb_bitfield_to_generic[i] >> 5)].value |= BIT(hid_kb_bitfield_to_generic[i] & 0x1F);
            }
        }
    }

    if (meta->kb_bitfield) {
        for (uint32_t i = 0; i < sizeof(meta->hid_axes_idx); i++) {
            if (meta->hid_axes_idx[i] > -1) {
                int32_t len = bt_data->reports[KB].usages[meta->hid_axes_idx[i]].bit_size;
                uint32_t offset = bt_data->reports[KB].usages[meta->hid_axes_idx[i]].bit_offset;
                uint32_t mask = (1ULL << len) - 1;
                uint32_t byte_offset = offset / 8;
                uint32_t bit_shift = offset % 8;
                uint32_t key_field = ((*(uint32_t *)(bt_data->base.input + byte_offset)) >> bit_shift) & mask;

                for (uint32_t mask = 1; mask; mask <<= 1) {
                    uint32_t key = __builtin_ffs(key_field & mask);
                    if (key) {
                        key = key - 1 + bt_data->reports[KB].usages[meta->hid_axes_idx[i]].usage;
                        ctrl_data->btns[(hid_kb_key_to_generic[key] >> 5)].value |= BIT(hid_kb_key_to_generic[key] & 0x1F);
                    }
                }
            }
        }
    }
    else {
        for (uint32_t i = 0; i < sizeof(meta->hid_axes_idx); i++) {
            if (meta->hid_axes_idx[i] > -1) {
                int32_t len = bt_data->reports[KB].usages[meta->hid_axes_idx[i]].bit_size;
                uint32_t offset = bt_data->reports[KB].usages[meta->hid_axes_idx[i]].bit_offset;
                uint32_t mask = (1ULL << len) - 1;
                uint32_t byte_offset = offset / 8;
                uint32_t bit_shift = offset % 8;
                uint32_t key = ((*(uint32_t *)(bt_data->base.input + byte_offset)) >> bit_shift) & mask;

                if (key > 3 && key < ARRAY_SIZE(hid_kb_key_to_generic)) {
                    ctrl_data->btns[(hid_kb_key_to_generic[key] >> 5)].value |= BIT(hid_kb_key_to_generic[key] & 0x1F);
                }
            }
        }
    }
}

static void hid_mouse_init(struct hid_report_meta *meta, struct hid_report *report, struct raw_src_mapping *map) {
    memset(meta->hid_axes_idx, -1, sizeof(meta->hid_axes_idx));
    meta->hid_btn_idx = -1;

    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        switch (report->usages[i].usage_page) {
            case USAGE_GEN_DESKTOP:
                switch (report->usages[i].usage) {
                    case USAGE_GEN_DESKTOP_X:
                        map->mask[0] |= BIT(MOUSE_X_LEFT) | BIT(MOUSE_X_RIGHT);
                        map->desc[0] |= BIT(MOUSE_X_LEFT) | BIT(MOUSE_X_RIGHT);
                        meta->hid_axes_idx[AXIS_RX] = i;
                        meta->hid_axes_meta[AXIS_RX].neutral = 0;
                        meta->hid_axes_meta[AXIS_RX].abs_max = report->usages[i].logical_max;
                        meta->hid_axes_meta[AXIS_RX].abs_min = report->usages[i].logical_min;
                        meta->hid_axes_meta[AXIS_RX].relative = 1;
                        break;
                    case USAGE_GEN_DESKTOP_Y:
                        map->mask[0] |= BIT(MOUSE_Y_DOWN) | BIT(MOUSE_Y_UP);
                        map->desc[0] |= BIT(MOUSE_Y_DOWN) | BIT(MOUSE_Y_UP);
                        meta->hid_axes_idx[AXIS_RY] = i;
                        meta->hid_axes_meta[AXIS_RY].neutral = 0;
                        meta->hid_axes_meta[AXIS_RY].abs_max = report->usages[i].logical_max;
                        meta->hid_axes_meta[AXIS_RY].abs_min = report->usages[i].logical_min;
                        meta->hid_axes_meta[AXIS_RY].polarity = 1;
                        meta->hid_axes_meta[AXIS_RY].relative = 1;
                        break;
                    case USAGE_GEN_DESKTOP_WHEEL:
                        map->mask[0] |= BIT(MOUSE_WY_DOWN) | BIT(MOUSE_WY_UP);
                        map->desc[0] |= BIT(MOUSE_WY_DOWN) | BIT(MOUSE_WY_UP);
                        meta->hid_axes_idx[AXIS_LY] = i;
                        meta->hid_axes_meta[AXIS_LY].neutral = 0;
                        meta->hid_axes_meta[AXIS_LY].abs_max = report->usages[i].logical_max;
                        meta->hid_axes_meta[AXIS_LY].abs_min = report->usages[i].logical_min;
                        meta->hid_axes_meta[AXIS_LY].polarity = 1;
                        meta->hid_axes_meta[AXIS_LY].relative = 1;
                        break;
                }
                break;
            case USAGE_GEN_BUTTON:
                meta->hid_btn_idx = i;
                for (uint32_t i = 0; i < report->usages[i].bit_size; i++) {
                    switch (i) {
                        case 0:
                            map->mask[0] |= BIT(MOUSE_LEFT);
                            map->btns_mask[MOUSE_LEFT] = BIT(i);
                            break;
                        case 1:
                            map->mask[0] |= BIT(MOUSE_RIGHT);
                            map->btns_mask[MOUSE_RIGHT] = BIT(i);
                            break;
                        case 2:
                            map->mask[0] |= BIT(MOUSE_MIDDLE);
                            map->btns_mask[MOUSE_MIDDLE] = BIT(i);
                            break;
                        case 7:
                            map->mask[0] |= BIT(MOUSE_8);
                            map->btns_mask[MOUSE_8] = BIT(i);
                            break;
                        default:
                            map->mask[0] |= BIT(MOUSE_4 + i - 3);
                            map->btns_mask[MOUSE_4 + i - 3] = BIT(i);
                            break;
                    }
                }
                break;
        }
    }
}

static void hid_mouse_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    struct hid_report_meta *meta = &devices_meta[bt_data->base.pids->id].reports_meta[MOUSE];

    if (!atomic_test_bit(&bt_data->base.flags[MOUSE], BT_INIT)) {
        hid_parser_load_report(bt_data, bt_data->base.report_id);
        hid_mouse_init(meta, &bt_data->reports[MOUSE], &bt_data->raw_src_mappings[MOUSE]);
        atomic_set_bit(&bt_data->base.flags[MOUSE], BT_INIT);
    }

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)bt_data->raw_src_mappings[MOUSE].mask;
    ctrl_data->desc = (uint32_t *)bt_data->raw_src_mappings[MOUSE].desc;

    if (meta->hid_btn_idx > -1) {
        uint32_t len = bt_data->reports[MOUSE].usages[meta->hid_btn_idx].bit_size;
        uint32_t offset = bt_data->reports[MOUSE].usages[meta->hid_btn_idx].bit_offset;
        uint32_t mask = (1ULL << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t buttons = ((*(uint32_t *)(bt_data->base.input + byte_offset)) >> bit_shift) & mask;

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (buttons & bt_data->raw_src_mappings[MOUSE].btns_mask[i]) {
                ctrl_data->btns[0].value |= generic_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (meta->hid_axes_idx[i] > -1) {
            int32_t len = bt_data->reports[MOUSE].usages[meta->hid_axes_idx[i]].bit_size;
            uint32_t offset = bt_data->reports[MOUSE].usages[meta->hid_axes_idx[i]].bit_offset;
            uint32_t mask = (1ULL << len) - 1;
            uint32_t byte_offset = offset / 8;
            uint32_t bit_shift = offset % 8;

            ctrl_data->axes[i].meta = &meta->hid_axes_meta[i];
            ctrl_data->axes[i].value = ((*(uint32_t *)(bt_data->base.input + byte_offset)) >> bit_shift) & mask;
            if (ctrl_data->axes[i].value & BIT(len - 1)) {
                ctrl_data->axes[i].value |= ~mask;
            }
        }
    }
}

static void hid_pad_init(struct hid_report_meta *meta, struct hid_report *report, struct raw_src_mapping *map) {
    uint32_t z_is_joy = 0;
    memset(meta->hid_axes_idx, -1, sizeof(meta->hid_axes_idx));
    meta->hid_btn_idx = -1;
    meta->hid_hat_idx = -1;

    /* Detect if Z axis is Right joystick or a trigger */
    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        if (report->usages[i].usage_page == USAGE_GEN_DESKTOP && report->usages[i].usage == USAGE_GEN_DESKTOP_X) {
            i++;
            if (report->usages[i].usage_page == USAGE_GEN_DESKTOP && report->usages[i].usage == USAGE_GEN_DESKTOP_Y) {
                i++;
                if (report->usages[i].usage_page == USAGE_GEN_DESKTOP && report->usages[i].usage == 0x33 /* USAGE_GEN_DESKTOP_RX */) {
                    z_is_joy = 0;
                    break;
                }
                else if (report->usages[i].usage_page == USAGE_GEN_DESKTOP && report->usages[i].usage == 0x32 /* USAGE_GEN_DESKTOP_Z */) {
                    z_is_joy = 1;
                    break;
                }
            }
        }
    }

    /* Build mask, desc, idx and meta base on usage */
    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        switch (report->usages[i].usage_page) {
            case USAGE_GEN_DESKTOP:
                switch (report->usages[i].usage) {
                    case USAGE_GEN_DESKTOP_X:
                        map->mask[0] |= BIT(PAD_LX_LEFT) | BIT(PAD_LX_RIGHT);
                        map->desc[0] |= BIT(PAD_LX_LEFT) | BIT(PAD_LX_RIGHT);
                        meta->hid_axes_idx[AXIS_LX] = i;
                        if (report->usages[i].logical_min >= 0) {
                            meta->hid_axes_meta[AXIS_LX].neutral = report->usages[i].logical_max / 2;
                            meta->hid_axes_meta[AXIS_LX].abs_max = report->usages[i].logical_max - meta->hid_axes_meta[AXIS_LX].neutral;
                            meta->hid_axes_meta[AXIS_LX].abs_min = meta->hid_axes_meta[AXIS_LX].neutral - report->usages[i].logical_min;
                        }
                        else {
                            meta->hid_axes_meta[AXIS_LX].neutral = 0;
                            meta->hid_axes_meta[AXIS_LX].abs_max = report->usages[i].logical_max;
                            meta->hid_axes_meta[AXIS_LX].abs_min = report->usages[i].logical_min;
                        }
                        break;
                    case USAGE_GEN_DESKTOP_Y:
                        map->mask[0] |= BIT(PAD_LY_DOWN) | BIT(PAD_LY_UP);
                        map->desc[0] |= BIT(PAD_LY_DOWN) | BIT(PAD_LY_UP);
                        meta->hid_axes_idx[AXIS_LY] = i;
                        if (report->usages[i].logical_min >= 0) {
                            meta->hid_axes_meta[AXIS_LY].neutral = report->usages[i].logical_max / 2;
                            meta->hid_axes_meta[AXIS_LY].abs_max = report->usages[i].logical_max - meta->hid_axes_meta[AXIS_LY].neutral;
                            meta->hid_axes_meta[AXIS_LY].abs_min = meta->hid_axes_meta[AXIS_LY].neutral - report->usages[i].logical_min;
                        }
                        else {
                            meta->hid_axes_meta[AXIS_LY].neutral = 0;
                            meta->hid_axes_meta[AXIS_LY].abs_max = report->usages[i].logical_max;
                            meta->hid_axes_meta[AXIS_LY].abs_min = report->usages[i].logical_min;
                        }
                        meta->hid_axes_meta[AXIS_LY].polarity = 1;
                        break;
                    case 0x32 /* USAGE_GEN_DESKTOP_Z */:
                        if (z_is_joy) {
                            map->mask[0] |= BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
                            map->desc[0] |= BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
                            meta->hid_axes_idx[AXIS_RX] = i;
                            if (report->usages[i].logical_min >= 0) {
                                meta->hid_axes_meta[AXIS_RX].neutral = report->usages[i].logical_max / 2;
                                meta->hid_axes_meta[AXIS_RX].abs_max = report->usages[i].logical_max - meta->hid_axes_meta[AXIS_RX].neutral;
                                meta->hid_axes_meta[AXIS_RX].abs_min = meta->hid_axes_meta[AXIS_RX].neutral - report->usages[i].logical_min;
                            }
                            else {
                                meta->hid_axes_meta[AXIS_RX].neutral = 0;
                                meta->hid_axes_meta[AXIS_RX].abs_max = report->usages[i].logical_max;
                                meta->hid_axes_meta[AXIS_RX].abs_min = report->usages[i].logical_min;
                            }
                        }
                        else {
                            map->mask[0] |= BIT(PAD_LM);
                            map->desc[0] |= BIT(PAD_LM);
                            meta->hid_axes_idx[TRIG_L] = i;
                            meta->hid_axes_meta[TRIG_L].abs_max = report->usages[i].logical_max;
                            meta->hid_axes_meta[TRIG_L].abs_min = report->usages[i].logical_min;
                            meta->hid_axes_meta[TRIG_L].neutral = report->usages[i].logical_min;
                        }
                        break;
                    case 0x33 /* USAGE_GEN_DESKTOP_RX */:
                        if (z_is_joy) {
                            map->mask[0] |= BIT(PAD_LM);
                            map->desc[0] |= BIT(PAD_LM);
                            meta->hid_axes_idx[TRIG_L] = i;
                            meta->hid_axes_meta[TRIG_L].abs_max = report->usages[i].logical_max;
                            meta->hid_axes_meta[TRIG_L].abs_min = report->usages[i].logical_min;
                            meta->hid_axes_meta[TRIG_L].neutral = report->usages[i].logical_min;
                        }
                        else {
                            map->mask[0] |= BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
                            map->desc[0] |= BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
                            meta->hid_axes_idx[AXIS_RX] = i;
                            if (report->usages[i].logical_min >= 0) {
                                meta->hid_axes_meta[AXIS_RX].neutral = report->usages[i].logical_max / 2;
                                meta->hid_axes_meta[AXIS_RX].abs_max = report->usages[i].logical_max - meta->hid_axes_meta[AXIS_RX].neutral;
                                meta->hid_axes_meta[AXIS_RX].abs_min = meta->hid_axes_meta[AXIS_RX].neutral - report->usages[i].logical_min;
                            }
                            else {
                                meta->hid_axes_meta[AXIS_RX].neutral = 0;
                                meta->hid_axes_meta[AXIS_RX].abs_max = report->usages[i].logical_max;
                                meta->hid_axes_meta[AXIS_RX].abs_min = report->usages[i].logical_min;
                            }
                        }
                        break;
                    case 0x34 /* USAGE_GEN_DESKTOP_RY */:
                        if (z_is_joy) {
                            map->mask[0] |= BIT(PAD_RM);
                            map->desc[0] |= BIT(PAD_RM);
                            meta->hid_axes_idx[TRIG_R] = i;
                            meta->hid_axes_meta[TRIG_R].abs_max = report->usages[i].logical_max;
                            meta->hid_axes_meta[TRIG_R].abs_min = report->usages[i].logical_min;
                            meta->hid_axes_meta[TRIG_R].neutral = report->usages[i].logical_min;
                        }
                        else {
                            map->mask[0] |= BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
                            map->desc[0] |= BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
                            meta->hid_axes_idx[AXIS_RY] = i;
                            if (report->usages[i].logical_min >= 0) {
                                meta->hid_axes_meta[AXIS_RY].neutral = report->usages[i].logical_max / 2;
                                meta->hid_axes_meta[AXIS_RY].abs_max = report->usages[i].logical_max - meta->hid_axes_meta[AXIS_RY].neutral;
                                meta->hid_axes_meta[AXIS_RY].abs_min = meta->hid_axes_meta[AXIS_RY].neutral - report->usages[i].logical_min;
                            }
                            else {
                                meta->hid_axes_meta[AXIS_RY].neutral = 0;
                                meta->hid_axes_meta[AXIS_RY].abs_max = report->usages[i].logical_max;
                                meta->hid_axes_meta[AXIS_RY].abs_min = report->usages[i].logical_min;
                            }
                            meta->hid_axes_meta[AXIS_RY].polarity = 1;
                        }
                        break;
                    case 0x35 /* USAGE_GEN_DESKTOP_RZ */:
                        if (z_is_joy) {
                            map->mask[0] |= BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
                            map->desc[0] |= BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
                            meta->hid_axes_idx[AXIS_RY] = i;
                            if (report->usages[i].logical_min >= 0) {
                                meta->hid_axes_meta[AXIS_RY].neutral = report->usages[i].logical_max / 2;
                                meta->hid_axes_meta[AXIS_RY].abs_max = report->usages[i].logical_max - meta->hid_axes_meta[AXIS_RY].neutral;
                                meta->hid_axes_meta[AXIS_RY].abs_min = meta->hid_axes_meta[AXIS_RY].neutral - report->usages[i].logical_min;
                            }
                            else {
                                meta->hid_axes_meta[AXIS_RY].neutral = 0;
                                meta->hid_axes_meta[AXIS_RY].abs_max = report->usages[i].logical_max;
                                meta->hid_axes_meta[AXIS_RY].abs_min = report->usages[i].logical_min;
                            }
                            meta->hid_axes_meta[AXIS_RY].polarity = 1;
                        }
                        else {
                            map->mask[0] |= BIT(PAD_RM);
                            map->desc[0] |= BIT(PAD_RM);
                            meta->hid_axes_idx[TRIG_R] = i;
                            meta->hid_axes_meta[TRIG_R].abs_max = report->usages[i].logical_max;
                            meta->hid_axes_meta[TRIG_R].abs_min = report->usages[i].logical_min;
                            meta->hid_axes_meta[TRIG_R].neutral = report->usages[i].logical_min;
                        }
                        break;
                    case 0x39 /* USAGE_GEN_DESKTOP_HAT */:
                        map->mask[0] |= BIT(PAD_LD_LEFT) | BIT(PAD_LD_RIGHT) | BIT(PAD_LD_DOWN) | BIT(PAD_LD_UP);
                        meta->hid_hat_idx = i;
                        break;
                }
                break;
            case USAGE_GEN_BUTTON:
                meta->hid_btn_idx = i;
                break;
            case 0x02 /* USAGE_SIMS */:
                switch (report->usages[i].usage) {
                    case 0xC4 /* USAGE_SIMS_ACCEL */:
                        map->mask[0] |= BIT(PAD_RM);
                        map->desc[0] |= BIT(PAD_RM);
                        meta->hid_axes_idx[TRIG_R] = i;
                        meta->hid_axes_meta[TRIG_R].abs_max = report->usages[i].logical_max;
                        meta->hid_axes_meta[TRIG_R].abs_min = report->usages[i].logical_min;
                        meta->hid_axes_meta[TRIG_R].neutral = report->usages[i].logical_min;
                        break;
                    case 0xC5 /* USAGE_SIMS_BRAKE */:
                        map->mask[0] |= BIT(PAD_LM);
                        map->desc[0] |= BIT(PAD_LM);
                        meta->hid_axes_idx[TRIG_L] = i;
                        meta->hid_axes_meta[TRIG_L].abs_max = report->usages[i].logical_max;
                        meta->hid_axes_meta[TRIG_L].abs_min = report->usages[i].logical_min;
                        meta->hid_axes_meta[TRIG_L].neutral = report->usages[i].logical_min;
                        break;
                }
                break;
        }
    }

    /* Add a little pull-back on axis max */
    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        meta->hid_axes_meta[i].abs_max *= MAX_PULL_BACK;
        meta->hid_axes_meta[i].abs_min *= MAX_PULL_BACK;
    }

    /* HID buttons order is from most important to the less in HID spec. */
    if (meta->hid_btn_idx > -1) {
        uint32_t hid_mask = (1 << report->usages[meta->hid_btn_idx].bit_size) - 1;

        /* Use a good default for most modern controller */
        for (uint32_t i = 12; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (hid_pad_default_btns_mask[i] && !(map->mask[0] & BIT(i))) {
                map->mask[0] |= BIT(i);
                map->btns_mask[i] = hid_pad_default_btns_mask[i];
                hid_mask &= ~hid_pad_default_btns_mask[i];
            }
        }

        /* fillup what is left */
        for (uint32_t hid_btn = 15, i = 12; hid_btn < report->usages[meta->hid_btn_idx].bit_size; hid_btn++) {
            while (map->btns_mask[i]) {
                i++;
                if (i > 32) {
                    goto fillup_end;
                }
            }
            if (!(map->mask[0] & BIT(i))) {
                map->mask[0] |= BIT(i);
                map->btns_mask[i] = BIT(hid_btn);
                hid_mask &= ~BIT(hid_btn);
            }
        }
fillup_end:
        ;
    }
}

static void hid_pad_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    struct hid_report_meta *meta = &devices_meta[bt_data->base.pids->id].reports_meta[PAD];

#ifdef CONFIG_BLUERETRO_ADAPTER_INPUT_DBG
#ifdef CONFIG_BLUERETRO_RAW_INPUT
    printf("{\"log_type\": \"wireless_input\", \"report_id\": %ld", bt_data->base.report_id);
#else
    printf("R%ld: ", bt_data->base.report_id);
#endif
#endif

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        hid_parser_load_report(bt_data, bt_data->base.report_id);
        hid_pad_init(meta, &bt_data->reports[PAD], &bt_data->raw_src_mappings[PAD]);
        mapping_quirks_apply(bt_data);
    }

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)bt_data->raw_src_mappings[PAD].mask;
    ctrl_data->desc = (uint32_t *)bt_data->raw_src_mappings[PAD].desc;

    if (meta->hid_btn_idx > -1) {
        uint32_t len = bt_data->reports[PAD].usages[meta->hid_btn_idx].bit_size;
        uint32_t offset = bt_data->reports[PAD].usages[meta->hid_btn_idx].bit_offset;
        uint32_t mask = (1ULL << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t buttons = ((*(uint32_t *)(bt_data->base.input + byte_offset)) >> bit_shift) & mask;

#ifdef CONFIG_BLUERETRO_RAW_INPUT
        printf(", \"btns\": %lu", buttons);
#endif

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (buttons & bt_data->raw_src_mappings[PAD].btns_mask[i]) {
                ctrl_data->btns[0].value |= generic_btns_mask[i];
            }
        }
    }

    /* Convert hat to regular btns */
    if (meta->hid_hat_idx > -1) {
        uint32_t len = bt_data->reports[PAD].usages[meta->hid_hat_idx].bit_size;
        uint32_t offset = bt_data->reports[PAD].usages[meta->hid_hat_idx].bit_offset;
        uint32_t mask = (1ULL << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t hat = ((*(uint32_t *)(bt_data->base.input + byte_offset)) >> bit_shift) & mask;
        uint32_t min = bt_data->reports[PAD].usages[meta->hid_hat_idx].logical_min;

#ifdef CONFIG_BLUERETRO_RAW_INPUT
        printf(", \"hat\": %lu", hat);
#endif

        ctrl_data->btns[0].value |= hat_to_ld_btns[(hat - min) & 0xF];
    }

#ifdef CONFIG_BLUERETRO_RAW_INPUT
    printf(", \"axes\": [");
#endif

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (meta->hid_axes_idx[i] > -1) {
            int32_t len = bt_data->reports[PAD].usages[meta->hid_axes_idx[i]].bit_size;
            uint32_t offset = bt_data->reports[PAD].usages[meta->hid_axes_idx[i]].bit_offset;
            uint32_t mask = (1ULL << len) - 1;
            uint32_t byte_offset = offset / 8;
            uint32_t bit_shift = offset % 8;
            uint32_t value = ((*(uint32_t *)(bt_data->base.input + byte_offset)) >> bit_shift) & mask;

#ifdef CONFIG_BLUERETRO_RAW_INPUT
                if (i) {
                    printf(", ");
                }
#endif

            if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
                bt_data->base.axes_cal[i] = -(value  - meta->hid_axes_meta[i].neutral);
            }

            ctrl_data->axes[i].meta = &meta->hid_axes_meta[i];

            /* Is axis unsign? */
            if (bt_data->reports[PAD].usages[meta->hid_axes_idx[i]].logical_min >= 0) {
                ctrl_data->axes[i].value = value - meta->hid_axes_meta[i].neutral + bt_data->base.axes_cal[i];
#ifdef CONFIG_BLUERETRO_RAW_INPUT
                printf("%lu", value);
#endif
            }
            else {
                ctrl_data->axes[i].value = value;
                if (ctrl_data->axes[i].value & BIT(len - 1)) {
                    ctrl_data->axes[i].value |= ~mask;
                }
#ifdef CONFIG_BLUERETRO_RAW_INPUT
                printf("%ld", ctrl_data->axes[i].value);
#endif
                ctrl_data->axes[i].value += bt_data->base.axes_cal[i];
            }
        }
    }
    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        atomic_set_bit(&bt_data->base.flags[PAD], BT_INIT);
    }
#ifdef CONFIG_BLUERETRO_RAW_INPUT
    printf("]}\n");
#endif

}

int32_t hid_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
#ifdef CONFIG_BLUERETRO_GENERIC_HID_DEBUG
    struct hid_report *report = hid_parser_get_report(bt_data->base.pids->id, bt_data->base.report_id);
    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        int32_t len = report->usages[i].bit_size;
        uint32_t offset = report->usages[i].bit_offset;
        uint32_t mask = (1ULL << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t value = ((*(uint32_t *)(bt_data->base.input + byte_offset)) >> bit_shift) & mask;
        if (report->usages[i].bit_size <= 4) {
            printf("R%ld %02lX%02lX: %s%01lX%s, ", bt_data->base.report_type, report->usages[i].usage_page, report->usages[i].usage, BOLD, value, RESET);
        }
        else if (report->usages[i].bit_size <= 8) {
            printf("R%ld %02lX%02lX: %s%02lX%s, ", bt_data->base.report_type, report->usages[i].usage_page, report->usages[i].usage, BOLD, value, RESET);
        }
        else if (report->usages[i].bit_size <= 12) {
            printf("R%ld %02lX%02lX: %s%03lX%s, ", bt_data->base.report_type, report->usages[i].usage_page, report->usages[i].usage, BOLD, value, RESET);
        }
        else if (report->usages[i].bit_size <= 16) {
            printf("R%ld %02lX%02lX: %s%04lX%s, ", bt_data->base.report_type, report->usages[i].usage_page, report->usages[i].usage, BOLD, value, RESET);
        }
        else if (report->usages[i].bit_size <= 32) {
            printf("R%ld %02lX%02lX: %s%08lX%s, ", bt_data->base.report_type, report->usages[i].usage_page, report->usages[i].usage, BOLD, value, RESET);
        }
    }
    printf("\n");
    return -1;
#else
    switch (bt_data->base.report_type) {
        case KB:
            hid_kb_to_generic(bt_data, ctrl_data);
            break;
        case MOUSE:
            hid_mouse_to_generic(bt_data, ctrl_data);
            break;
        case PAD:
            hid_pad_to_generic(bt_data, ctrl_data);
            break;
        default:
            printf("# Unsupported report type: %02lX\n", bt_data->base.report_type);
            return -1;
    }
#endif

    return 0;
}

void hid_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data) {
    struct generic_rumble *rumble = (struct generic_rumble *)bt_data->base.output;

    rumble->report_size = 0;
    uint32_t bytes_count = 0;
    uint32_t tmp_value = 0;
    uint32_t offset = 0;
    uint32_t counter = 0;
    bool is_rumble_usage = false;

    for (uint32_t i = 0; i < bt_data->reports[RUMBLE].usage_cnt; i++)
    {
        is_rumble_usage = false;

        switch (bt_data->reports[RUMBLE].usages[i].usage)
        {
            case 0x50: /* Duration */
                bytes_count = (bt_data->reports[RUMBLE].usages[i].bit_size + 7) / 8;
                rumble->report_size += bytes_count;

                if (fb_data->state) {
                    tmp_value = bt_data->reports[RUMBLE].usages[i].logical_max;
                }
                else {
                    tmp_value = bt_data->reports[RUMBLE].usages[i].logical_min;
                }

                is_rumble_usage = true;
                break;
            case 0x70: /* Magnitude */
            case 0x97: /* Enable Actuators */
                bytes_count = (bt_data->reports[RUMBLE].usages[i].bit_size + 7) / 8;
                rumble->report_size += bytes_count;

                if (fb_data->state) {
                    tmp_value = bt_data->reports[RUMBLE].usages[i].logical_max * RUMBLE_ON_MULTIPLIER;
                }
                else {
                    tmp_value = bt_data->reports[RUMBLE].usages[i].logical_min;
                }

                is_rumble_usage = true;
                break;
            case 0x7C: /* Loop Count */
                bytes_count = (bt_data->reports[RUMBLE].usages[i].bit_size + 7) / 8;
                rumble->report_size += bytes_count;

                if (fb_data->state) {
                    if (fb_data->cycles) {
                        tmp_value = fb_data->cycles;
                    }
                    else {
                        tmp_value = bt_data->reports[RUMBLE].usages[i].logical_max;
                    }
                }
                else {
                    tmp_value = bt_data->reports[RUMBLE].usages[i].logical_min;
                }

                is_rumble_usage = true;
                break;
            case 0xA7: /* Start Delay */
                bytes_count = (bt_data->reports[RUMBLE].usages[i].bit_size + 7) / 8;
                rumble->report_size += bytes_count;

                tmp_value = fb_data->start;

                is_rumble_usage = true;
                break;
        }

        if (is_rumble_usage) {
            counter = 0;
            while(tmp_value)
            {
                rumble->state[offset++] = tmp_value;
                tmp_value >>= 8;
                counter++;
            }
            for (uint32_t refill = counter; refill < bytes_count; refill++) {
                rumble->state[offset++] = 0;
            }
        }
    }

    rumble->report_id = bt_data->reports[RUMBLE].id;
}

