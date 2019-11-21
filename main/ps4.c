#include <string.h>
#include "zephyr/types.h"
#include "util.h"
#include "ps4.h"

enum {
    PS4_D_UP,
    PS4_D_RIGHT,
    PS4_D_DOWN,
    PS4_D_LEFT,
    PS4_S,
    PS4_X,
    PS4_C,
    PS4_T,
    PS4_L1,
    PS4_R1,
    PS4_L2,
    PS4_R2,
    PS4_SHARE,
    PS4_OPTIONS,
    PS4_L3,
    PS4_R3,
    PS4_PS,
    PS4_TP,
};

const uint8_t ps4_axes_idx[6] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       2,       3,       7,      8
};

const struct ctrl_meta ps4_btn_meta =
{
    .polarity = 0,
};

const struct ctrl_meta ps4_axes_meta[6] =
{
    {.neutral = 0x80, .abs_max = 0x80},
    {.neutral = 0x80, .abs_max = 0x80, .polarity = 1},
    {.neutral = 0x80, .abs_max = 0x80},
    {.neutral = 0x80, .abs_max = 0x80, .polarity = 1},
    {.neutral = 0x00, .abs_max = 0xFF},
    {.neutral = 0x00, .abs_max = 0xFF},
};

struct ps4_map {
    uint8_t reserved[2];
    union {
        struct {
            uint8_t reserved2[4];
            uint32_t buttons;
        };
        uint8_t axes[9];
    };
} __packed;

const uint32_t ps4_mask[4] = {0xBB7F0FFF, 0x00000000, 0x00000000, 0x00000000};
const uint32_t ps4_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};

const uint32_t ps4_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(PS4_D_LEFT), BIT(PS4_D_RIGHT), BIT(PS4_D_DOWN), BIT(PS4_D_UP),
    0, 0, 0, 0,
    BIT(PS4_S), BIT(PS4_C), BIT(PS4_X), BIT(PS4_T),
    BIT(PS4_OPTIONS), BIT(PS4_SHARE), BIT(PS4_PS), BIT(PS4_TP),
    0, BIT(PS4_L1), 0, BIT(PS4_L3),
    0, BIT(PS4_R2), 0, BIT(PS4_R3),
};

void ps4_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    struct ps4_map *map = (struct ps4_map *)bt_data->input;

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)ps4_mask;
    ctrl_data->desc = (uint32_t *)ps4_desc;

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (map->buttons & ps4_btns_mask[i]) {
            ctrl_data->btns[0].value |= generic_btns_mask[i];
        }
    }

    if (!atomic_test_bit(&bt_data->flags, BT_INIT)) {
        for (uint32_t i = 0; i < ARRAY_SIZE(map->axes); i++) {
            bt_data->axes_cal[i] = -(map->axes[ps4_axes_idx[i]] - ps4_axes_meta[i].neutral);
        }
        atomic_set_bit(&bt_data->flags, BT_INIT);
    }

    for (uint32_t i = 0; i < ARRAY_SIZE(map->axes); i++) {
        ctrl_data->axes[i].meta = &ps4_axes_meta[i];
        ctrl_data->axes[i].value = map->axes[ps4_axes_idx[i]] - ps4_axes_meta[i].neutral + bt_data->axes_cal[i];
    }

}
