#include <string.h>
#include "../zephyr/types.h"
#include "../util.h"
#include "sw.h"

#define BT_HIDP_SW_SUBCMD_SET_LED 0x30
#define SW_AXES_MAX 4

enum {
    SW_B,
    SW_A,
    SW_Y,
    SW_X,
    SW_L,
    SW_R,
    SW_ZL,
    SW_ZR,
    SW_MINUS,
    SW_PLUS,
    SW_LJ,
    SW_RJ,
    SW_HOME,
    SW_CAPTURE,
    SW_SL,
    SW_SR,
};

static const uint8_t led_dev_id_map[] = {
    0x1, 0x2, 0x4, 0x8, 0x3, 0x6, 0xC
};

const uint8_t sw_axes_idx[SW_AXES_MAX] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY  */
    0,       1,       2,       3
};

const struct ctrl_meta sw_btn_meta =
{
    .polarity = 0,
};

const struct ctrl_meta sw_axes_meta[SW_AXES_MAX] =
{
    {.neutral = 0x8000, .abs_max = 0x5EEC, .deadzone = 0xB00},
    {.neutral = 0x8000, .abs_max = 0x5EEC, .deadzone = 0xB00, .polarity = 1},
    {.neutral = 0x8000, .abs_max = 0x5EEC, .deadzone = 0xB00},
    {.neutral = 0x8000, .abs_max = 0x5EEC, .deadzone = 0xB00, .polarity = 1},
};

struct sw_map {
    uint16_t buttons;
    uint8_t hat;
    uint16_t axes[4];
} __packed;

struct sw_conf {
    uint8_t tid;
    uint8_t rumble[8];
    uint8_t subcmd;
    uint8_t subcmd_data[38];
} __packed;

static const uint8_t sw_rumble_on[] = {0x28, 0x88, 0x60, 0x61, 0x28, 0x88, 0x60, 0x61};
static const uint8_t sw_rumble_off[] = {0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40};

const uint32_t sw_mask[4] = {0xFFFF0FFF, 0x00000000, 0x00000000, 0x00000000};
const uint32_t sw_desc[4] = {0x000000FF, 0x00000000, 0x00000000, 0x00000000};

const uint32_t sw_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(SW_Y), BIT(SW_A), BIT(SW_B), BIT(SW_X),
    BIT(SW_PLUS), BIT(SW_MINUS), BIT(SW_HOME), BIT(SW_CAPTURE),
    BIT(SW_ZL), BIT(SW_L), BIT(SW_SL), BIT(SW_LJ),
    BIT(SW_ZR), BIT(SW_R), BIT(SW_SR), BIT(SW_RJ),
};

void sw_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    struct sw_map *map = (struct sw_map *)bt_data->input;

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)sw_mask;
    ctrl_data->desc = (uint32_t *)sw_desc;

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (map->buttons & sw_btns_mask[i]) {
            ctrl_data->btns[0].value |= generic_btns_mask[i];
        }
    }

    /* Convert hat to regular btns */
    ctrl_data->btns[0].value |= hat_to_ld_btns[map->hat & 0xF];

    if (!atomic_test_bit(&bt_data->flags, BT_INIT)) {
        for (uint32_t i = 0; i < SW_AXES_MAX; i++) {
            bt_data->axes_cal[i] = -(map->axes[sw_axes_idx[i]] - sw_axes_meta[i].neutral);
        }
        atomic_set_bit(&bt_data->flags, BT_INIT);
    }

    for (uint32_t i = 0; i < SW_AXES_MAX; i++) {
        ctrl_data->axes[i].meta = &sw_axes_meta[i];
        ctrl_data->axes[i].value = map->axes[sw_axes_idx[i]] - sw_axes_meta[i].neutral + bt_data->axes_cal[i];
    }
}

void sw_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data) {
    struct sw_conf *conf = (struct sw_conf *)bt_data->output;
    memset((void *)conf, 0, sizeof(*conf));

    conf->subcmd = BT_HIDP_SW_SUBCMD_SET_LED;
    conf->subcmd_data[0] = led_dev_id_map[bt_data->dev_id];

    if (fb_data->state) {
        memcpy((void *)conf->rumble, (void *)sw_rumble_on, sizeof(sw_rumble_on));
    }
    else {
        memcpy((void *)conf->rumble, (void *)sw_rumble_off, sizeof(sw_rumble_off));
    }
}
