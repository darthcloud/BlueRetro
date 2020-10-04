/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../zephyr/types.h"
#include "../zephyr/usb_hid.h"
#include "../util.h"
#include "hid_generic.h"

struct hid_report_meta {
    int8_t hid_btn_idx;
    int8_t hid_axes_idx[ADAPTER_MAX_AXES];
    int8_t hid_hat_idx;
    struct ctrl_meta hid_axes_meta[ADAPTER_MAX_AXES];
    uint32_t hid_mask[4];
    uint32_t hid_desc[4];
    uint32_t hid_btns_mask[32];
};

struct hid_reports_meta {
    struct hid_report_meta reports_meta[REPORT_MAX];
};

static struct hid_reports_meta devices_meta[BT_MAX_DEV] = {0};

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

static void hid_kb_init(struct hid_report_meta *meta, struct hid_report *report) {
    memset(meta->hid_axes_idx, -1, sizeof(meta->hid_axes_idx));
    meta->hid_btn_idx = -1;

    for (uint32_t i = 0, key_idx = 0; i < report->usage_cnt; i++) {
        switch (report->usages[i].usage_page) {
            case USAGE_GEN_KEYBOARD:
                if (report->usages[i].usage >= 0xE0 && report->usages[i].usage <= 0xE7) {
                    meta->hid_mask[0] |= BIT(KB_LWIN & 0x1F) | BIT(KB_LCTRL & 0x1F) | BIT(KB_LSHIFT & 0x1F);
                    meta->hid_mask[3] |= BIT(KB_LALT & 0x1F) | BIT(KB_RCTRL & 0x1F) | BIT(KB_RSHIFT & 0x1F) | BIT(KB_RALT & 0x1F) | BIT(KB_RWIN & 0x1F);
                    meta->hid_btn_idx = i;
                }
                else if (key_idx < 6) {
                    meta->hid_mask[0] |= 0xBBBFFFFF;
                    meta->hid_mask[1] |= 0xFFFFFFFF;
                    meta->hid_mask[2] |= 0xFFFFFFFF;
                    meta->hid_mask[3] |= 0x3FFF;
                    meta->hid_axes_idx[key_idx] = i;
                    key_idx++;
                }
                break;
        }
    }
}

static void hid_kb_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    struct hid_report_meta *meta = &devices_meta[bt_data->dev_id].reports_meta[KB];

    if (!atomic_test_bit(&bt_data->reports[KB].flags, BT_INIT)) {
        hid_kb_init(meta, &bt_data->reports[KB]);
        atomic_set_bit(&bt_data->reports[KB].flags, BT_INIT);
    }

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)meta->hid_mask;
    ctrl_data->desc = (uint32_t *)meta->hid_desc;

    if (meta->hid_btn_idx > -1) {
        uint32_t len = bt_data->reports[KB].usages[meta->hid_btn_idx].bit_size;
        uint32_t offset = bt_data->reports[KB].usages[meta->hid_btn_idx].bit_offset;
        uint32_t mask = (1 << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t buttons = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;

        for (uint8_t i = 0, mask = 1; mask; i++, mask <<= 1) {
            if (buttons & mask) {
                ctrl_data->btns[(hid_kb_bitfield_to_generic[i] >> 5)].value |= BIT(hid_kb_bitfield_to_generic[i] & 0x1F);
            }
        }
    }

    for (uint32_t i = 0; i < sizeof(meta->hid_axes_idx); i++) {
        if (meta->hid_axes_idx[i] > -1) {
            int32_t len = bt_data->reports[KB].usages[meta->hid_axes_idx[i]].bit_size;
            uint32_t offset = bt_data->reports[KB].usages[meta->hid_axes_idx[i]].bit_offset;
            uint32_t mask = (1 << len) - 1;
            uint32_t byte_offset = offset / 8;
            uint32_t bit_shift = offset % 8;
            uint32_t key = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;

            if (key > 3 && key < ARRAY_SIZE(hid_kb_key_to_generic)) {
                ctrl_data->btns[(hid_kb_key_to_generic[key] >> 5)].value |= BIT(hid_kb_key_to_generic[key] & 0x1F);
            }
        }
    }
}

static void hid_mouse_init(struct hid_report_meta *meta, struct hid_report *report) {
    memset(meta->hid_axes_idx, -1, sizeof(meta->hid_axes_idx));
    meta->hid_btn_idx = -1;

    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        switch (report->usages[i].usage_page) {
            case USAGE_GEN_DESKTOP:
                switch (report->usages[i].usage) {
                    case USAGE_GEN_DESKTOP_X:
                        meta->hid_mask[0] |= BIT(MOUSE_X_LEFT) | BIT(MOUSE_X_RIGHT);
                        meta->hid_desc[0] |= BIT(MOUSE_X_LEFT) | BIT(MOUSE_X_RIGHT);
                        meta->hid_axes_idx[AXIS_RX] = i;
                        meta->hid_axes_meta[AXIS_RX].neutral = 0;
                        meta->hid_axes_meta[AXIS_RX].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        break;
                    case USAGE_GEN_DESKTOP_Y:
                        meta->hid_mask[0] |= BIT(MOUSE_Y_DOWN) | BIT(MOUSE_Y_UP);
                        meta->hid_desc[0] |= BIT(MOUSE_Y_DOWN) | BIT(MOUSE_Y_UP);
                        meta->hid_axes_idx[AXIS_RY] = i;
                        meta->hid_axes_meta[AXIS_RY].neutral = 0;
                        meta->hid_axes_meta[AXIS_RY].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        meta->hid_axes_meta[AXIS_RY].polarity = 1;
                        break;
                    case USAGE_GEN_DESKTOP_WHEEL:
                        meta->hid_mask[0] |= BIT(MOUSE_WY_DOWN) | BIT(MOUSE_WY_UP);
                        meta->hid_desc[0] |= BIT(MOUSE_WY_DOWN) | BIT(MOUSE_WY_UP);
                        meta->hid_axes_idx[AXIS_LY] = i;
                        meta->hid_axes_meta[AXIS_LY].neutral = 0;
                        meta->hid_axes_meta[AXIS_LY].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        meta->hid_axes_meta[AXIS_LY].polarity = 1;
                        break;
                }
                break;
            case USAGE_GEN_BUTTON:
                meta->hid_btn_idx = i;
                for (uint32_t i = 0; i < report->usages[i].bit_size; i++) {
                    switch (i) {
                        case 0:
                            meta->hid_mask[0] |= BIT(MOUSE_LEFT);
                            meta->hid_btns_mask[MOUSE_LEFT] = BIT(i);
                            break;
                        case 1:
                            meta->hid_mask[0] |= BIT(MOUSE_RIGHT);
                            meta->hid_btns_mask[MOUSE_RIGHT] = BIT(i);
                            break;
                        case 2:
                            meta->hid_mask[0] |= BIT(MOUSE_MIDDLE);
                            meta->hid_btns_mask[MOUSE_MIDDLE] = BIT(i);
                            break;
                        case 7:
                            meta->hid_mask[0] |= BIT(MOUSE_8);
                            meta->hid_btns_mask[MOUSE_8] = BIT(i);
                            break;
                        default:
                            meta->hid_mask[0] |= BIT(MOUSE_4 + i - 3);
                            meta->hid_btns_mask[MOUSE_4 + i - 3] = BIT(i);
                            break;
                    }
                }
                break;
        }
    }
}

static void hid_mouse_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    struct hid_report_meta *meta = &devices_meta[bt_data->dev_id].reports_meta[MOUSE];

    if (!atomic_test_bit(&bt_data->reports[MOUSE].flags, BT_INIT)) {
        hid_mouse_init(meta, &bt_data->reports[MOUSE]);
        atomic_set_bit(&bt_data->reports[MOUSE].flags, BT_INIT);
    }

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)meta->hid_mask;
    ctrl_data->desc = (uint32_t *)meta->hid_desc;

    if (meta->hid_btn_idx > -1) {
        uint32_t len = bt_data->reports[MOUSE].usages[meta->hid_btn_idx].bit_size;
        uint32_t offset = bt_data->reports[MOUSE].usages[meta->hid_btn_idx].bit_offset;
        uint32_t mask = (1 << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t buttons = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (buttons & meta->hid_btns_mask[i]) {
                ctrl_data->btns[0].value |= generic_btns_mask[i];
            }
        }
    }

    if (!atomic_test_bit(&bt_data->flags, BT_INIT)) {
        for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
            bt_data->axes_cal[i] = meta->hid_axes_meta[i].neutral;
        }
        atomic_set_bit(&bt_data->flags, BT_INIT);
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (meta->hid_axes_idx[i] > -1) {
            int32_t len = bt_data->reports[MOUSE].usages[meta->hid_axes_idx[i]].bit_size;
            uint32_t offset = bt_data->reports[MOUSE].usages[meta->hid_axes_idx[i]].bit_offset;
            uint32_t mask = (1 << len) - 1;
            uint32_t byte_offset = offset / 8;
            uint32_t bit_shift = offset % 8;

            ctrl_data->axes[i].meta = &meta->hid_axes_meta[i];
            ctrl_data->axes[i].value = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;
            if (ctrl_data->axes[i].value & BIT(len - 1)) {
                ctrl_data->axes[i].value |= ~mask;
            }
        }
    }
}

static void hid_pad_init(struct hid_report_meta *meta, struct hid_report *report) {
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
                        meta->hid_mask[0] |= BIT(PAD_LX_LEFT) | BIT(PAD_LX_RIGHT);
                        meta->hid_desc[0] |= BIT(PAD_LX_LEFT) | BIT(PAD_LX_RIGHT);
                        meta->hid_axes_idx[AXIS_LX] = i;
                        meta->hid_axes_meta[AXIS_LX].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        meta->hid_axes_meta[AXIS_LX].neutral = meta->hid_axes_meta[AXIS_LX].abs_max;
                        break;
                    case USAGE_GEN_DESKTOP_Y:
                        meta->hid_mask[0] |= BIT(PAD_LY_DOWN) | BIT(PAD_LY_UP);
                        meta->hid_desc[0] |= BIT(PAD_LY_DOWN) | BIT(PAD_LY_UP);
                        meta->hid_axes_idx[AXIS_LY] = i;
                        meta->hid_axes_meta[AXIS_LY].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        meta->hid_axes_meta[AXIS_LY].neutral = meta->hid_axes_meta[AXIS_LY].abs_max;
                        meta->hid_axes_meta[AXIS_LY].polarity = 1;
                        break;
                    case 0x32 /* USAGE_GEN_DESKTOP_Z */:
                        if (z_is_joy) {
                            meta->hid_mask[0] |= BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
                            meta->hid_desc[0] |= BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
                            meta->hid_axes_idx[AXIS_RX] = i;
                            meta->hid_axes_meta[AXIS_RX].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            meta->hid_axes_meta[AXIS_RX].neutral = meta->hid_axes_meta[AXIS_RX].abs_max;
                        }
                        else {
                            meta->hid_mask[0] |= BIT(PAD_LM);
                            meta->hid_desc[0] |= BIT(PAD_LM);
                            meta->hid_axes_idx[TRIG_L] = i;
                            meta->hid_axes_meta[TRIG_L].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            meta->hid_axes_meta[TRIG_L].neutral = meta->hid_axes_meta[TRIG_L].abs_max;
                        }
                        break;
                    case 0x33 /* USAGE_GEN_DESKTOP_RX */:
                        if (z_is_joy) {
                            meta->hid_mask[0] |= BIT(PAD_LM);
                            meta->hid_desc[0] |= BIT(PAD_LM);
                            meta->hid_axes_idx[TRIG_L] = i;
                            meta->hid_axes_meta[TRIG_L].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            meta->hid_axes_meta[TRIG_L].neutral = meta->hid_axes_meta[TRIG_L].abs_max;
                        }
                        else {
                            meta->hid_mask[0] |= BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
                            meta->hid_desc[0] |= BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
                            meta->hid_axes_idx[AXIS_RX] = i;
                            meta->hid_axes_meta[AXIS_RX].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            meta->hid_axes_meta[AXIS_RX].neutral = meta->hid_axes_meta[AXIS_RX].abs_max;
                        }
                        break;
                    case 0x34 /* USAGE_GEN_DESKTOP_RY */:
                        if (z_is_joy) {
                            meta->hid_mask[0] |= BIT(PAD_RM);
                            meta->hid_desc[0] |= BIT(PAD_RM);
                            meta->hid_axes_idx[TRIG_R] = i;
                            meta->hid_axes_meta[TRIG_R].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            meta->hid_axes_meta[TRIG_R].neutral = meta->hid_axes_meta[TRIG_R].abs_max;
                        }
                        else {
                            meta->hid_mask[0] |= BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
                            meta->hid_desc[0] |= BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
                            meta->hid_axes_idx[AXIS_RY] = i;
                            meta->hid_axes_meta[AXIS_RY].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            meta->hid_axes_meta[AXIS_RY].neutral = meta->hid_axes_meta[AXIS_RY].abs_max;
                            meta->hid_axes_meta[AXIS_LY].polarity = 1;
                        }
                        break;
                    case 0x35 /* USAGE_GEN_DESKTOP_RZ */:
                        if (z_is_joy) {
                            meta->hid_mask[0] |= BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
                            meta->hid_desc[0] |= BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
                            meta->hid_axes_idx[AXIS_RY] = i;
                            meta->hid_axes_meta[AXIS_RY].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            meta->hid_axes_meta[AXIS_RY].neutral = meta->hid_axes_meta[AXIS_RY].abs_max;
                            meta->hid_axes_meta[AXIS_LY].polarity = 1;
                        }
                        else {
                            meta->hid_mask[0] |= BIT(PAD_RM);
                            meta->hid_desc[0] |= BIT(PAD_RM);
                            meta->hid_axes_idx[TRIG_R] = i;
                            meta->hid_axes_meta[TRIG_R].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            meta->hid_axes_meta[TRIG_R].neutral = meta->hid_axes_meta[TRIG_R].abs_max;
                        }
                        break;
                    case 0x39 /* USAGE_GEN_DESKTOP_HAT */:
                        meta->hid_mask[0] |= BIT(PAD_LD_LEFT) | BIT(PAD_LD_RIGHT) | BIT(PAD_LD_DOWN) | BIT(PAD_LD_UP);
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
                        meta->hid_mask[0] |= BIT(PAD_RM);
                        meta->hid_desc[0] |= BIT(PAD_RM);
                        meta->hid_axes_idx[TRIG_R] = i;
                        meta->hid_axes_meta[TRIG_R].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        meta->hid_axes_meta[TRIG_R].neutral = meta->hid_axes_meta[TRIG_R].abs_max;
                        break;
                    case 0xC5 /* USAGE_SIMS_BRAKE */:
                        meta->hid_mask[0] |= BIT(PAD_LM);
                        meta->hid_desc[0] |= BIT(PAD_LM);
                        meta->hid_axes_idx[TRIG_L] = i;
                        meta->hid_axes_meta[TRIG_L].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        meta->hid_axes_meta[TRIG_L].neutral = meta->hid_axes_meta[TRIG_L].abs_max;
                        break;
                }
                break;
        }
    }

    /* HID buttons order is from most important to the less in HID spec. */
    /* That don't really help us so we just assign them to unused buttons. */
    for (uint32_t mask = (1U << 16), btn = 0, i = 16; mask && btn < report->usages[meta->hid_btn_idx].bit_size; mask <<= 1, i++) {
        if (!(meta->hid_mask[0] & mask)) {
            meta->hid_mask[0] |= mask;
            meta->hid_btns_mask[i] = BIT(btn);
            btn++;
        }
    }
}

static void hid_pad_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    struct hid_report_meta *meta = &devices_meta[bt_data->dev_id].reports_meta[PAD];

    if (!atomic_test_bit(&bt_data->reports[PAD].flags, BT_INIT)) {
        hid_pad_init(meta, &bt_data->reports[PAD]);
        atomic_set_bit(&bt_data->reports[PAD].flags, BT_INIT);
    }

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)meta->hid_mask;
    ctrl_data->desc = (uint32_t *)meta->hid_desc;

    if (meta->hid_btn_idx > -1) {
        uint32_t len = bt_data->reports[PAD].usages[meta->hid_btn_idx].bit_size;
        uint32_t offset = bt_data->reports[PAD].usages[meta->hid_btn_idx].bit_offset;
        uint32_t mask = (1 << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t buttons = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (buttons & meta->hid_btns_mask[i]) {
                ctrl_data->btns[0].value |= generic_btns_mask[i];
            }
        }
    }

    /* Convert hat to regular btns */
    if (meta->hid_hat_idx > -1) {
        uint32_t len = bt_data->reports[PAD].usages[meta->hid_hat_idx].bit_size;
        uint32_t offset = bt_data->reports[PAD].usages[meta->hid_hat_idx].bit_offset;
        uint32_t mask = (1 << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t hat = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;
        uint32_t min = bt_data->reports[PAD].usages[meta->hid_hat_idx].logical_min;

        ctrl_data->btns[0].value |= hat_to_ld_btns[(hat - min) & 0xF];
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (meta->hid_axes_idx[i] > -1) {
            int32_t len = bt_data->reports[PAD].usages[meta->hid_axes_idx[i]].bit_size;
            uint32_t offset = bt_data->reports[PAD].usages[meta->hid_axes_idx[i]].bit_offset;
            uint32_t mask = (1 << len) - 1;
            uint32_t byte_offset = offset / 8;
            uint32_t bit_shift = offset % 8;
            uint32_t value = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;

            if (!atomic_test_bit(&bt_data->flags, BT_INIT)) {
                bt_data->axes_cal[i] = -(value  - meta->hid_axes_meta[i].neutral);
            }

            ctrl_data->axes[i].meta = &meta->hid_axes_meta[i];

            /* Is axis sign? */
            if (bt_data->reports[PAD].usages[meta->hid_axes_idx[i]].logical_min >= 0) {
                ctrl_data->axes[i].value = value - meta->hid_axes_meta[i].neutral + bt_data->axes_cal[i];
            }
            else {
                ctrl_data->axes[i].value = value;
                if (ctrl_data->axes[i].value & BIT(len - 1)) {
                    ctrl_data->axes[i].value |= ~mask;
                }
                ctrl_data->axes[i].value += bt_data->axes_cal[i];
            }
        }
    }
    if (!atomic_test_bit(&bt_data->flags, BT_INIT)) {
        atomic_set_bit(&bt_data->flags, BT_INIT);
    }
}

//#define HID_DEBUG
void hid_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
#ifdef HID_DEBUG
    struct hid_report *report = &bt_data->reports[bt_data->report_type];
    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        int32_t len = report->usages[i].bit_size;
        uint32_t offset = report->usages[i].bit_offset;
        uint32_t mask = (1 << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t value = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;
        if (report->usages[i].bit_size <= 4) {
            printf("%02X%02X: %s%X%s, ", report->usages[i].usage_page, report->usages[i].usage, BOLD, value, RESET);
        }
        else if (report->usages[i].bit_size <= 8) {
            printf("%02X%02X: %s%02X%s, ", report->usages[i].usage_page, report->usages[i].usage, BOLD, value, RESET);
        }
        else if (report->usages[i].bit_size <= 12) {
            printf("%02X%02X: %s%03X%s, ", report->usages[i].usage_page, report->usages[i].usage, BOLD, value, RESET);
        }
        else if (report->usages[i].bit_size <= 16) {
            printf("%02X%02X: %s%04X%s, ", report->usages[i].usage_page, report->usages[i].usage, BOLD, value, RESET);
        }
        else if (report->usages[i].bit_size <= 32) {
            printf("%02X%02X: %s%08X%s, ", report->usages[i].usage_page, report->usages[i].usage, BOLD, value, RESET);
        }
    }
    printf("\n");
#else
    switch (bt_data->report_type) {
        case KB:
            hid_kb_to_generic(bt_data, ctrl_data);
            break;
        case MOUSE:
            hid_mouse_to_generic(bt_data, ctrl_data);
            break;
        case PAD:
            hid_pad_to_generic(bt_data, ctrl_data);
            break;
        case EXTRA:
            break;
        default:
            printf("# Unknown report type: %02X\n", bt_data->report_type);
            break;
    }
#endif
}
