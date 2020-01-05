#include <string.h>
#include "../zephyr/types.h"
#include "../util.h"
#include "wii.h"

enum {
    WIIU_R = 1,
    WIIU_PLUS,
    WIIU_HOME,
    WIIU_MINUS,
    WIIU_L,
    WIIU_D_DOWN,
    WIIU_D_RIGHT,
    WIIU_D_UP,
    WIIU_D_LEFT,
    WIIU_ZR,
    WIIU_X,
    WIIU_A,
    WIIU_Y,
    WIIU_B,
    WIIU_ZL,
    WIIU_RJ,
    WIIU_LJ,
};

const uint8_t led_dev_id_map[] = {
    0x1, 0x2, 0x4, 0x8, 0x3, 0x6, 0xC
};

const uint8_t wiiu_axes_idx[4] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY  */
    0,       2,       1,       3
};

const struct ctrl_meta wiiu_btn_meta =
{
    .polarity = 1,
};

const struct ctrl_meta wiiu_axes_meta =
{
    .neutral = 0x800,
    .abs_max = 0x44C,
};

struct wiiu_map {
    uint8_t reserved[5];
    uint16_t axes[4];
    uint32_t buttons;
} __packed;

const uint32_t wiiu_mask[4] = {0xBB7F0FFF, 0x00000000, 0x00000000, 0x00000000};
const uint32_t wiiu_desc[4] = {0x000000FF, 0x00000000, 0x00000000, 0x00000000};

const uint32_t wiiu_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(WIIU_D_LEFT), BIT(WIIU_D_RIGHT), BIT(WIIU_D_DOWN), BIT(WIIU_D_UP),
    0, 0, 0, 0,
    BIT(WIIU_Y), BIT(WIIU_A), BIT(WIIU_B), BIT(WIIU_X),
    BIT(WIIU_PLUS), BIT(WIIU_MINUS), BIT(WIIU_HOME), 0,
    BIT(WIIU_ZL), BIT(WIIU_L), 0, BIT(WIIU_LJ),
    BIT(WIIU_ZR), BIT(WIIU_R), 0, BIT(WIIU_RJ),
};

void wiiu_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    struct wiiu_map *map = (struct wiiu_map *)bt_data->input;

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)wiiu_mask;
    ctrl_data->desc = (uint32_t *)wiiu_desc;

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (~map->buttons & wiiu_btns_mask[i]) {
            ctrl_data->btns[0].value |= generic_btns_mask[i];
        }
    }

    if (!atomic_test_bit(&bt_data->flags, BT_INIT)) {
        for (uint32_t i = 0; i < ARRAY_SIZE(map->axes); i++) {
            bt_data->axes_cal[i] = -(map->axes[wiiu_axes_idx[i]] - wiiu_axes_meta.neutral);
        }
        atomic_set_bit(&bt_data->flags, BT_INIT);
    }

    for (uint32_t i = 0; i < ARRAY_SIZE(map->axes); i++) {
        ctrl_data->axes[i].meta = &wiiu_axes_meta;
        ctrl_data->axes[i].value = map->axes[wiiu_axes_idx[i]] - wiiu_axes_meta.neutral + bt_data->axes_cal[i];
    }

}

void wii_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data) {
    if (fb_data->state) {
        bt_data->output[0] = (led_dev_id_map[bt_data->dev_id] << 4) | 0x01;
    }
    else {
        bt_data->output[0] = (led_dev_id_map[bt_data->dev_id] << 4);
    }
}
