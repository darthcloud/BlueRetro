#include <string.h>
#include "zephyr/types.h"
#include "util.h"
#include "xb1.h"

enum {
    XB1_XBOX,
    XB1_A = XB1_XBOX,
    XB1_B,
    XB1_X,
    XB1_Y,
    XB1_LB,
    XB1_RB,
    XB1_VIEW,
    XB1_MENU,
    XB1_LJ,
    XB1_RJ,
};

const uint8_t xb1_axes_idx[6] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       2,       3,       4,      5
};

const struct ctrl_meta xb1_btn_meta =
{
    .polarity = 0,
};

const struct ctrl_meta xb1_axes_meta[6] =
{
    {.neutral = 0x8000, .abs_max = 0x8000},
    {.neutral = 0x8000, .abs_max = 0x8000, .polarity = 1},
    {.neutral = 0x8000, .abs_max = 0x8000},
    {.neutral = 0x8000, .abs_max = 0x8000, .polarity = 1},
    {.neutral = 0x0000, .abs_max = 0x3FF},
    {.neutral = 0x0000, .abs_max = 0x3FF},
};

struct xb1_map {
    uint16_t axes[6];
    uint8_t hat;
    uint16_t buttons;
} __packed;

const uint32_t xb1_mask[4] = {0xAA7F0FFF, 0x00000000, 0x00000000, 0x00000000};
const uint32_t xb1_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};

const uint32_t xb1_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(XB1_X), BIT(XB1_B), BIT(XB1_A), BIT(XB1_Y),
    BIT(XB1_MENU), BIT(XB1_VIEW), 0, 0,
    0, BIT(XB1_LB), 0, BIT(XB1_LJ),
    0, BIT(XB1_RB), 0, BIT(XB1_RJ),
};

void xb1_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    struct xb1_map *map = (struct xb1_map *)bt_data->input;

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)xb1_mask;
    ctrl_data->desc = (uint32_t *)xb1_desc;

    if (bt_data->report_id == 0x01) {
        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (map->buttons & xb1_btns_mask[i]) {
                ctrl_data->btns[0].value |= generic_btns_mask[i];
            }
        }

        /* Convert hat to regular btns */
        ctrl_data->btns[0].value |= hat_to_ld_btns[(map->hat & 0xF) - 1];

        if (!atomic_test_bit(&bt_data->flags, BT_INIT)) {
            for (uint32_t i = 0; i < ARRAY_SIZE(map->axes); i++) {
                bt_data->axes_cal[i] = -(map->axes[xb1_axes_idx[i]] - xb1_axes_meta[i].neutral);
            }
            atomic_set_bit(&bt_data->flags, BT_INIT);
        }

        for (uint32_t i = 0; i < ARRAY_SIZE(map->axes); i++) {
            ctrl_data->axes[i].meta = &xb1_axes_meta[i];
            ctrl_data->axes[i].value = map->axes[xb1_axes_idx[i]] - xb1_axes_meta[i].neutral + bt_data->axes_cal[i];
        }
    }
    else if (bt_data->report_id == 0x02) {
        if (bt_data->input[0] & BIT(XB1_XBOX)) {
            ctrl_data->btns[0].value |= BIT(PAD_MT);
        }
    }
}
