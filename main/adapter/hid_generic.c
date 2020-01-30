#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../zephyr/types.h"
#include "../zephyr/usb_hid.h"
#include "../util.h"
#include "hid_generic.h"

static int8_t hid_axes_idx[REPORT_MAX][6];
static struct ctrl_meta hid_axes_meta[REPORT_MAX][6] = {0};
static uint32_t hid_mask[REPORT_MAX][4] = {0};
static uint32_t hid_desc[REPORT_MAX][4] = {0};
static uint32_t hid_btns_mask[REPORT_MAX][32] = {0};

static void hid_kb_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
}

static void hid_mouse_init(struct hid_report *report) {
    memset(hid_axes_idx[MOUSE], -1, sizeof(hid_axes_idx[MOUSE]));

    for (uint32_t i = 0; i < REPORT_MAX_USAGE; i++) {
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
}

static void hid_pad_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
}

void hid_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
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
#if 0
    struct hid_map *map = (struct hid_map *)bt_data->input;

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->desc = (uint32_t *)hid_desc;

    if (bt_data->report_id == 0x01) {
        ctrl_data->mask = (uint32_t *)hid_mask;

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (map->buttons & hid_btns_mask[i]) {
                ctrl_data->btns[0].value |= generic_btns_mask[i];
            }
        }

        /* Convert hat to regular btns */
        ctrl_data->btns[0].value |= hat_to_ld_btns[(map->hat & 0xF) - 1];

        if (!atomic_test_bit(&bt_data->flags, BT_INIT)) {
            for (uint32_t i = 0; i < ARRAY_SIZE(map->axes); i++) {
                bt_data->axes_cal[i] = -(map->axes[hid_axes_idx[i]] - hid_axes_meta[i].neutral);
            }
            atomic_set_bit(&bt_data->flags, BT_INIT);
        }

        for (uint32_t i = 0; i < ARRAY_SIZE(map->axes); i++) {
            ctrl_data->axes[i].meta = &hid_axes_meta[i];
            ctrl_data->axes[i].value = map->axes[hid_axes_idx[i]] - hid_axes_meta[i].neutral + bt_data->axes_cal[i];
        }
    }
    else if (bt_data->report_id == 0x02) {
        ctrl_data->mask = (uint32_t *)hid_mask2;

        if (bt_data->input[0] & BIT(HID_XBOX)) {
            ctrl_data->btns[0].value |= BIT(PAD_MT);
        }
    }
#endif
}
