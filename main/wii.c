#include "zephyr/types.h"
#include "util.h"
#include "adapter.h"

enum {
    WIIU_R = 1,
    WIIU_PLUS,
    WIIU_HOME,
    WIIU_MINUS,
    WIIU_L,
    WIIU_DOWN,
    WIIU_RIGHT,
    WIIU_UP,
    WIIU_LEFT,
    WIIU_ZR,
    WIIU_X,
    WIIU_A,
    WIIU_Y,
    WIIU_B,
    WIIU_ZL,
    WIIU_RJ,
    WIIU_LJ,
};

struct wiiu_map {
    uint8_t reserved[5];
    uint16_t lx_axis;
    uint16_t rx_axis;
    uint16_t ly_axis;
    uint16_t ry_axis;
    uint32_t buttons;
} __packed;

const uint32_t wiiu_btns_mask[32] =
{
    BIT(WIIU_UP),    /* DU */
    BIT(WIIU_LEFT),  /* DL */
    BIT(WIIU_RIGHT), /* DR */
    BIT(WIIU_DOWN),  /* DD */
    0, 0, 0, 0,      /* LU, LL, LR, LD */
    BIT(WIIU_X),     /* BU */
    BIT(WIIU_Y),     /* BL */
    BIT(WIIU_A),     /* BR */
    BIT(WIIU_B),     /* BD */
    0, 0, 0, 0,      /* RU, RL, RR, RD */
    0,               /* LA */
    BIT(WIIU_ZL),    /* LM */
    BIT(WIIU_L),     /* LS */
    BIT(WIIU_LJ),    /* LJ */
    0,               /* RA */
    BIT(WIIU_ZR),    /* RM */
    BIT(WIIU_R),     /* RS */
    BIT(WIIU_RJ),    /* RJ */
    BIT(WIIU_PLUS),  /* MM */
    BIT(WIIU_MINUS), /* MS */
    BIT(WIIU_HOME),  /* MT */
};

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
