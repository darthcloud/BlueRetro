#include <string.h>
#include "zephyr/types.h"
#include "util.h"
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
    .deadzone = 0x00F,
    .abs_btn_thrs = 0x250,
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
    BIT(WIIU_A), BIT(WIIU_Y), BIT(WIIU_B), BIT(WIIU_X),
    BIT(WIIU_PLUS), BIT(WIIU_MINUS), BIT(WIIU_HOME), 0,
    BIT(WIIU_ZL), BIT(WIIU_L), 0, BIT(WIIU_LJ),
    BIT(WIIU_ZR), BIT(WIIU_R), 0, BIT(WIIU_RJ),
};

#if 0
void wiiu_init_desc(struct bt_data *bt_data) {
    struct wiiu_map *map = (struct wiiu_map *)bt_data->input;

    bt_data->ctrl_desc.data[BTNS0].type = U32_TYPE;
    bt_data->ctrl_desc.data[BTNS0].pval.u32 = &map->buttons;

    bt_data->ctrl_desc.data[LX_AXIS].type = U16_TYPE;
    bt_data->ctrl_desc.data[LX_AXIS].pval.u16 = &map->lx_axis;

    bt_data->ctrl_desc.data[LY_AXIS].type = U16_TYPE;
    bt_data->ctrl_desc.data[LY_AXIS].pval.u16 = &map->ly_axis;

    bt_data->ctrl_desc.data[RX_AXIS].type = U16_TYPE;
    bt_data->ctrl_desc.data[RX_AXIS].pval.u16 = &map->rx_axis;

    bt_data->ctrl_desc.data[RY_AXIS].type = U16_TYPE;
    bt_data->ctrl_desc.data[RY_AXIS].pval.u16 = &map->ry_axis;
}
#endif

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
    for (uint32_t i = 0; i < ARRAY_SIZE(map->axes); i++) {
        ctrl_data->axes[i].meta = &wiiu_axes_meta;
        ctrl_data->axes[i].value = map->axes[wiiu_axes_idx[i]] - wiiu_axes_meta.neutral;
    }
}
