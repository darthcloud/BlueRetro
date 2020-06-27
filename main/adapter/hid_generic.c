#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../zephyr/types.h"
#include "../zephyr/usb_hid.h"
#include "../util.h"
#include "hid_generic.h"

//TODO This should be a struct for each report type and for each bt dev.
static int8_t hid_btn_idx[REPORT_MAX];
static int8_t hid_axes_idx[REPORT_MAX][6];
static int8_t hid_hat_idx[REPORT_MAX];
static struct ctrl_meta hid_axes_meta[REPORT_MAX][6] = {0};
static uint32_t hid_mask[REPORT_MAX][4] = {0};
static uint32_t hid_desc[REPORT_MAX][4] = {0};
static uint32_t hid_btns_mask[REPORT_MAX][32] = {0};

static void hid_kb_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
}

static void hid_mouse_init(struct hid_report *report) {
    memset(hid_axes_idx[MOUSE], -1, sizeof(hid_axes_idx[MOUSE]));
    hid_btn_idx[MOUSE] = -1;

    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        switch (report->usages[i].usage_page) {
            case USAGE_GEN_DESKTOP:
                switch (report->usages[i].usage) {
                    case USAGE_GEN_DESKTOP_X:
                        hid_mask[MOUSE][0] |= BIT(MOUSE_X_LEFT) | BIT(MOUSE_X_RIGHT);
                        hid_desc[MOUSE][0] |= BIT(MOUSE_X_LEFT) | BIT(MOUSE_X_RIGHT);
                        hid_axes_idx[MOUSE][AXIS_RX] = i;
                        hid_axes_meta[MOUSE][AXIS_RX].neutral = 0;
                        hid_axes_meta[MOUSE][AXIS_RX].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        break;
                    case USAGE_GEN_DESKTOP_Y:
                        hid_mask[MOUSE][0] |= BIT(MOUSE_Y_DOWN) | BIT(MOUSE_Y_UP);
                        hid_desc[MOUSE][0] |= BIT(MOUSE_Y_DOWN) | BIT(MOUSE_Y_UP);
                        hid_axes_idx[MOUSE][AXIS_RY] = i;
                        hid_axes_meta[MOUSE][AXIS_RY].neutral = 0;
                        hid_axes_meta[MOUSE][AXIS_RY].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        hid_axes_meta[MOUSE][AXIS_RY].polarity = 1;
                        break;
                    case USAGE_GEN_DESKTOP_WHEEL:
                        hid_mask[MOUSE][0] |= BIT(MOUSE_WY_DOWN) | BIT(MOUSE_WY_UP);
                        hid_desc[MOUSE][0] |= BIT(MOUSE_WY_DOWN) | BIT(MOUSE_WY_UP);
                        hid_axes_idx[MOUSE][AXIS_LY] = i;
                        hid_axes_meta[MOUSE][AXIS_LY].neutral = 0;
                        hid_axes_meta[MOUSE][AXIS_LY].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        hid_axes_meta[MOUSE][AXIS_LY].polarity = 1;
                        break;
                }
                break;
            case USAGE_GEN_BUTTON:
                hid_btn_idx[MOUSE] = i;
                for (uint32_t i = 0; i < report->usages[i].bit_size; i++) {
                    switch (i) {
                        case 0:
                            hid_mask[MOUSE][0] |= BIT(MOUSE_LEFT);
                            hid_btns_mask[MOUSE][MOUSE_LEFT] = BIT(i);
                            break;
                        case 1:
                            hid_mask[MOUSE][0] |= BIT(MOUSE_RIGHT);
                            hid_btns_mask[MOUSE][MOUSE_RIGHT] = BIT(i);
                            break;
                        case 2:
                            hid_mask[MOUSE][0] |= BIT(MOUSE_MIDDLE);
                            hid_btns_mask[MOUSE][MOUSE_MIDDLE] = BIT(i);
                            break;
                        case 7:
                            hid_mask[MOUSE][0] |= BIT(MOUSE_8);
                            hid_btns_mask[MOUSE][MOUSE_8] = BIT(i);
                            break;
                        default:
                            hid_mask[MOUSE][0] |= BIT(MOUSE_4 + i - 3);
                            hid_btns_mask[MOUSE][MOUSE_4 + i - 3] = BIT(i);
                            break;
                    }
                }
                break;
        }
    }
}

static void hid_mouse_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    if (!atomic_test_bit(&bt_data->reports[MOUSE].flags, BT_INIT)) {
        hid_mouse_init(&bt_data->reports[MOUSE]);
        atomic_set_bit(&bt_data->reports[MOUSE].flags, BT_INIT);
    }

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)hid_mask[MOUSE];
    ctrl_data->desc = (uint32_t *)hid_desc[MOUSE];

    if (hid_btn_idx[MOUSE] > -1) {
        uint32_t len = bt_data->reports[MOUSE].usages[hid_btn_idx[MOUSE]].bit_size;
        uint32_t offset = bt_data->reports[MOUSE].usages[hid_btn_idx[MOUSE]].bit_offset;
        uint32_t mask = (1 << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t buttons = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (buttons & hid_btns_mask[MOUSE][i]) {
                ctrl_data->btns[0].value |= generic_btns_mask[i];
            }
        }
    }

    if (!atomic_test_bit(&bt_data->flags, BT_INIT)) {
        for (uint32_t i = 0; i < sizeof(hid_axes_idx[MOUSE]); i++) {
            bt_data->axes_cal[i] = hid_axes_meta[MOUSE][i].neutral;
        }
        atomic_set_bit(&bt_data->flags, BT_INIT);
    }

    for (uint32_t i = 0; i < sizeof(hid_axes_idx[MOUSE]); i++) {
        if (hid_axes_idx[MOUSE][i] > -1) {
            int32_t len = bt_data->reports[MOUSE].usages[hid_axes_idx[MOUSE][i]].bit_size;
            uint32_t offset = bt_data->reports[MOUSE].usages[hid_axes_idx[MOUSE][i]].bit_offset;
            uint32_t mask = (1 << len) - 1;
            uint32_t byte_offset = offset / 8;
            uint32_t bit_shift = offset % 8;

            ctrl_data->axes[i].meta = &hid_axes_meta[MOUSE][i];
            ctrl_data->axes[i].value = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;
            if (ctrl_data->axes[i].value & BIT(len - 1)) {
                ctrl_data->axes[i].value |= ~mask;
            }
        }
    }
}

static void hid_pad_init(struct hid_report *report) {
    uint32_t z_is_joy = 0;
    memset(hid_axes_idx[PAD], -1, sizeof(hid_axes_idx[PAD]));
    hid_btn_idx[PAD] = -1;
    hid_hat_idx[PAD] = -1;

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
                        hid_mask[PAD][0] |= BIT(PAD_LX_LEFT) | BIT(PAD_LX_RIGHT);
                        hid_desc[PAD][0] |= BIT(PAD_LX_LEFT) | BIT(PAD_LX_RIGHT);
                        hid_axes_idx[PAD][AXIS_LX] = i;
                        hid_axes_meta[PAD][AXIS_LX].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        hid_axes_meta[PAD][AXIS_LX].neutral = hid_axes_meta[PAD][AXIS_LX].abs_max;
                        break;
                    case USAGE_GEN_DESKTOP_Y:
                        hid_mask[PAD][0] |= BIT(PAD_LY_DOWN) | BIT(PAD_LY_UP);
                        hid_desc[PAD][0] |= BIT(PAD_LY_DOWN) | BIT(PAD_LY_UP);
                        hid_axes_idx[PAD][AXIS_LY] = i;
                        hid_axes_meta[PAD][AXIS_LY].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        hid_axes_meta[PAD][AXIS_LY].neutral = hid_axes_meta[PAD][AXIS_LY].abs_max;
                        hid_axes_meta[PAD][AXIS_LY].polarity = 1;
                        break;
                    case 0x32 /* USAGE_GEN_DESKTOP_Z */:
                        if (z_is_joy) {
                            hid_mask[PAD][0] |= BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
                            hid_desc[PAD][0] |= BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
                            hid_axes_idx[PAD][AXIS_RX] = i;
                            hid_axes_meta[PAD][AXIS_RX].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            hid_axes_meta[PAD][AXIS_RX].neutral = hid_axes_meta[PAD][AXIS_RX].abs_max;
                        }
                        else {
                            hid_mask[PAD][0] |= BIT(PAD_LM);
                            hid_desc[PAD][0] |= BIT(PAD_LM);
                            hid_axes_idx[PAD][TRIG_L] = i;
                            hid_axes_meta[PAD][TRIG_L].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            hid_axes_meta[PAD][TRIG_L].neutral = hid_axes_meta[PAD][TRIG_L].abs_max;
                        }
                        break;
                    case 0x33 /* USAGE_GEN_DESKTOP_RX */:
                        if (z_is_joy) {
                            hid_mask[PAD][0] |= BIT(PAD_LM);
                            hid_desc[PAD][0] |= BIT(PAD_LM);
                            hid_axes_idx[PAD][TRIG_L] = i;
                            hid_axes_meta[PAD][TRIG_L].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            hid_axes_meta[PAD][TRIG_L].neutral = hid_axes_meta[PAD][TRIG_L].abs_max;
                        }
                        else {
                            hid_mask[PAD][0] |= BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
                            hid_desc[PAD][0] |= BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
                            hid_axes_idx[PAD][AXIS_RX] = i;
                            hid_axes_meta[PAD][AXIS_RX].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            hid_axes_meta[PAD][AXIS_RX].neutral = hid_axes_meta[PAD][AXIS_RX].abs_max;
                        }
                        break;
                    case 0x34 /* USAGE_GEN_DESKTOP_RY */:
                        if (z_is_joy) {
                            hid_mask[PAD][0] |= BIT(PAD_RM);
                            hid_desc[PAD][0] |= BIT(PAD_RM);
                            hid_axes_idx[PAD][TRIG_R] = i;
                            hid_axes_meta[PAD][TRIG_R].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            hid_axes_meta[PAD][TRIG_R].neutral = hid_axes_meta[PAD][TRIG_R].abs_max;
                        }
                        else {
                            hid_mask[PAD][0] |= BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
                            hid_desc[PAD][0] |= BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
                            hid_axes_idx[PAD][AXIS_RY] = i;
                            hid_axes_meta[PAD][AXIS_RY].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            hid_axes_meta[PAD][AXIS_RY].neutral = hid_axes_meta[PAD][AXIS_RY].abs_max;
                            hid_axes_meta[PAD][AXIS_LY].polarity = 1;
                        }
                        break;
                    case 0x35 /* USAGE_GEN_DESKTOP_RZ */:
                        if (z_is_joy) {
                            hid_mask[PAD][0] |= BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
                            hid_desc[PAD][0] |= BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
                            hid_axes_idx[PAD][AXIS_RY] = i;
                            hid_axes_meta[PAD][AXIS_RY].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            hid_axes_meta[PAD][AXIS_RY].neutral = hid_axes_meta[PAD][AXIS_RY].abs_max;
                            hid_axes_meta[PAD][AXIS_LY].polarity = 1;
                        }
                        else {
                            hid_mask[PAD][0] |= BIT(PAD_RM);
                            hid_desc[PAD][0] |= BIT(PAD_RM);
                            hid_axes_idx[PAD][TRIG_R] = i;
                            hid_axes_meta[PAD][TRIG_R].abs_max = pow(2, report->usages[i].bit_size) / 2;
                            hid_axes_meta[PAD][TRIG_R].neutral = hid_axes_meta[PAD][TRIG_R].abs_max;
                        }
                        break;
                    case 0x39 /* USAGE_GEN_DESKTOP_HAT */:
                        hid_mask[PAD][0] |= BIT(PAD_LD_LEFT) | BIT(PAD_LD_RIGHT) | BIT(PAD_LD_DOWN) | BIT(PAD_LD_UP);
                        hid_hat_idx[PAD] = i;
                        break;
                }
                break;
            case USAGE_GEN_BUTTON:
                hid_btn_idx[PAD] = i;
                break;
            case 0x02 /* USAGE_SIMS */:
                switch (report->usages[i].usage) {
                    case 0xC4 /* USAGE_SIMS_ACCEL */:
                        hid_mask[PAD][0] |= BIT(PAD_RM);
                        hid_desc[PAD][0] |= BIT(PAD_RM);
                        hid_axes_idx[PAD][TRIG_R] = i;
                        hid_axes_meta[PAD][TRIG_R].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        hid_axes_meta[PAD][TRIG_R].neutral = hid_axes_meta[PAD][TRIG_R].abs_max;
                        break;
                    case 0xC5 /* USAGE_SIMS_BRAKE */:
                        hid_mask[PAD][0] |= BIT(PAD_LM);
                        hid_desc[PAD][0] |= BIT(PAD_LM);
                        hid_axes_idx[PAD][TRIG_L] = i;
                        hid_axes_meta[PAD][TRIG_L].abs_max = pow(2, report->usages[i].bit_size) / 2;
                        hid_axes_meta[PAD][TRIG_L].neutral = hid_axes_meta[PAD][TRIG_L].abs_max;
                        break;
                }
                break;
        }
    }

    /* HID buttons order is from most important to the less in HID spec. */
    /* That don't really help us so we just assign them to unused buttons. */
    for (uint32_t mask = (1U << 16), btn = 0, i = 16; mask && btn < report->usages[hid_btn_idx[PAD]].bit_size; mask <<= 1, i++) {
        if (!(hid_mask[PAD][0] & mask)) {
            hid_mask[PAD][0] |= mask;
            hid_btns_mask[PAD][i] = BIT(btn);
            btn++;
        }
    }
}

static void hid_pad_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    if (!atomic_test_bit(&bt_data->reports[PAD].flags, BT_INIT)) {
        hid_pad_init(&bt_data->reports[PAD]);
        atomic_set_bit(&bt_data->reports[PAD].flags, BT_INIT);
    }

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)hid_mask[PAD];
    ctrl_data->desc = (uint32_t *)hid_desc[PAD];

    if (hid_btn_idx[PAD] > -1) {
        uint32_t len = bt_data->reports[PAD].usages[hid_btn_idx[PAD]].bit_size;
        uint32_t offset = bt_data->reports[PAD].usages[hid_btn_idx[PAD]].bit_offset;
        uint32_t mask = (1 << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t buttons = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (buttons & hid_btns_mask[PAD][i]) {
                ctrl_data->btns[0].value |= generic_btns_mask[i];
            }
        }
    }

    /* Convert hat to regular btns */
    if (hid_hat_idx[PAD] > -1) {
        uint32_t len = bt_data->reports[PAD].usages[hid_hat_idx[PAD]].bit_size;
        uint32_t offset = bt_data->reports[PAD].usages[hid_hat_idx[PAD]].bit_offset;
        uint32_t mask = (1 << len) - 1;
        uint32_t byte_offset = offset / 8;
        uint32_t bit_shift = offset % 8;
        uint32_t hat = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;
        uint32_t min = bt_data->reports[PAD].usages[hid_hat_idx[PAD]].logical_min;

        ctrl_data->btns[0].value |= hat_to_ld_btns[(hat - min) & 0xF];
    }

    for (uint32_t i = 0; i < sizeof(hid_axes_idx[PAD]); i++) {
        if (hid_axes_idx[PAD][i] > -1) {
            int32_t len = bt_data->reports[PAD].usages[hid_axes_idx[PAD][i]].bit_size;
            uint32_t offset = bt_data->reports[PAD].usages[hid_axes_idx[PAD][i]].bit_offset;
            uint32_t mask = (1 << len) - 1;
            uint32_t byte_offset = offset / 8;
            uint32_t bit_shift = offset % 8;
            uint32_t value = ((*(uint32_t *)(bt_data->input + byte_offset)) >> bit_shift) & mask;

            if (!atomic_test_bit(&bt_data->flags, BT_INIT)) {
                bt_data->axes_cal[i] = -(value  - hid_axes_meta[PAD][i].neutral);
            }

            ctrl_data->axes[i].meta = &hid_axes_meta[PAD][i];

            /* Is axis sign? */
            if (bt_data->reports[PAD].usages[hid_axes_idx[PAD][i]].logical_min >= 0) {
                ctrl_data->axes[i].value = value - hid_axes_meta[PAD][i].neutral + bt_data->axes_cal[i];
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
