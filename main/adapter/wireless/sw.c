/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/mapping_quirks.h"
#include "bluetooth/hidp/sw.h"
#include "sw.h"

#define BT_HIDP_SW_SUBCMD_SET_LED 0x30
#define SW_AXES_MAX 4

enum {
    SW_B = 0,
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

enum {
    SW_N_Y = 8,
    SW_N_X,
    SW_N_B,
    SW_N_A,
    SW_N_R_SR,
    SW_N_R_SL,
    SW_N_R,
    SW_N_ZR,
    SW_N_MINUS,
    SW_N_PLUS,
    SW_N_RJ,
    SW_N_LJ,
    SW_N_HOME,
    SW_N_CAPTURE,
    SW_N_DOWN = 24,
    SW_N_UP,
    SW_N_RIGHT,
    SW_N_LEFT,
    SW_N_L_SR,
    SW_N_L_SL,
    SW_N_L,
    SW_N_ZL,
};

static const uint8_t led_dev_id_map[] = {
    0x1, 0x2, 0x4, 0x8, 0x3, 0x6, 0xC
};

static const uint8_t sw_axes_idx[SW_AXES_MAX] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY  */
    0,       1,       2,       3
};

static const uint8_t sw_jc_axes_idx[SW_AXES_MAX] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY  */
    1,       0,       3,       2
};

static const struct ctrl_meta sw_axes_meta[SW_AXES_MAX] =
{
    {.neutral = 0x8000, .abs_max = 0x5EEC, .deadzone = 0xB00},
    {.neutral = 0x8000, .abs_max = 0x5EEC, .deadzone = 0xB00, .polarity = 1},
    {.neutral = 0x8000, .abs_max = 0x5EEC, .deadzone = 0xB00},
    {.neutral = 0x8000, .abs_max = 0x5EEC, .deadzone = 0xB00, .polarity = 1},
};

static const struct ctrl_meta sw_3rd_axes_meta[SW_AXES_MAX] =
{
    {.neutral = 0x8000, .abs_max = 0x8000},
    {.neutral = 0x8000, .abs_max = 0x8000, .polarity = 1},
    {.neutral = 0x8000, .abs_max = 0x8000},
    {.neutral = 0x8000, .abs_max = 0x8000, .polarity = 1},
};

static const struct ctrl_meta sw_native_axes_meta_default[SW_AXES_MAX] =
{
    {.neutral = 0x800, .abs_max = 0x578, .deadzone = 0xAE},
    {.neutral = 0x800, .abs_max = 0x578, .deadzone = 0xAE},
    {.neutral = 0x800, .abs_max = 0x578, .deadzone = 0xAE},
    {.neutral = 0x800, .abs_max = 0x578, .deadzone = 0xAE},
};

static struct ctrl_meta sw_native_axes_meta[BT_MAX_DEV][SW_AXES_MAX] = {0};

struct sw_map {
    uint16_t buttons;
    uint8_t hat;
    uint16_t axes[4];
} __packed;

struct sw_native_map {
    uint8_t timer;
    uint32_t buttons;
    uint8_t axes[6];
} __packed;

struct sw_conf {
    uint8_t tid;
    uint8_t rumble[8];
    uint8_t subcmd;
    uint8_t subcmd_data[38];
} __packed;

static const uint8_t sw_rumble_on[] = {0x28, 0x88, 0x60, 0x61, 0x28, 0x88, 0x60, 0x61};
static const uint8_t sw_rumble_off[] = {0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40};

static const uint32_t sw_mask[4] = {0xFFFF0FFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_desc[4] = {0x000000FF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_jc_mask[4] = {0xFFFF0F0F, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_jc_desc[4] = {0x0000000F, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_gen_mask[4] = {0x22FF0F00, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_gen_desc[4] = {0x0000000F, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_n64_mask[4] = {0x33D50FFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_n64_desc[4] = {0x0000000F, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_legacy_mask[4] = {0xFFFF0F00, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_legacy_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};

static const uint32_t sw_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(SW_Y), BIT(SW_A), BIT(SW_B), BIT(SW_X),
    BIT(SW_PLUS), BIT(SW_MINUS), BIT(SW_HOME), BIT(SW_CAPTURE),
    BIT(SW_ZL), BIT(SW_L), BIT(SW_SL), BIT(SW_LJ),
    BIT(SW_ZR), BIT(SW_R), BIT(SW_SR), BIT(SW_RJ),
};

static const uint32_t sw_native_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(SW_N_LEFT), BIT(SW_N_RIGHT), BIT(SW_N_DOWN), BIT(SW_N_UP),
    0, 0, 0, 0,
    BIT(SW_N_Y), BIT(SW_N_A), BIT(SW_N_B), BIT(SW_N_X),
    BIT(SW_N_PLUS), BIT(SW_N_MINUS), BIT(SW_N_HOME), BIT(SW_N_CAPTURE),
    BIT(SW_N_ZL), BIT(SW_N_L), BIT(SW_N_L_SL) | BIT(SW_N_R_SL), BIT(SW_N_LJ),
    BIT(SW_N_ZR), BIT(SW_N_R), BIT(SW_N_L_SR) | BIT(SW_N_R_SR), BIT(SW_N_RJ),
};

static const uint32_t sw_gen_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(SW_N_LEFT), BIT(SW_N_RIGHT), BIT(SW_N_DOWN), BIT(SW_N_UP),
    0, 0, 0, 0,
    BIT(SW_N_A), BIT(SW_N_R), BIT(SW_N_B), BIT(SW_N_Y),
    BIT(SW_N_PLUS), BIT(SW_N_ZR), BIT(SW_N_HOME), BIT(SW_N_CAPTURE),
    0, BIT(SW_N_X), 0, 0,
    0, BIT(SW_N_L), 0, 0,
};

static const uint32_t sw_n64_btns_mask[32] = {
    0, 0, 0, 0,
    BIT(SW_N_X), BIT(SW_N_MINUS), BIT(SW_N_ZR), BIT(SW_N_Y),
    BIT(SW_N_LEFT), BIT(SW_N_RIGHT), BIT(SW_N_DOWN), BIT(SW_N_UP),
    0, 0, 0, 0,
    BIT(SW_N_B), 0, BIT(SW_N_A), 0,
    BIT(SW_N_PLUS), 0, BIT(SW_N_HOME), BIT(SW_N_CAPTURE),
    BIT(SW_N_ZL), BIT(SW_N_L), 0, 0,
    BIT(SW_N_LJ), BIT(SW_N_R), 0, 0,
};

static const uint32_t sw_admiral_btns_mask[32] = {
    0, 0, 0, 0,
    BIT(SW_MINUS), BIT(SW_RJ), BIT(SW_CAPTURE), BIT(SW_HOME),
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(SW_B), 0, BIT(SW_A), 0,
    BIT(SW_PLUS), 0, 0, 0,
    BIT(SW_SL), BIT(SW_L), 0, 0,
    0, BIT(SW_R), 0, 0,
};

static void sw_native_pad_init(struct bt_data *bt_data) {
    struct bt_hid_sw_ctrl_calib *calib;
    struct ctrl_meta *meta = sw_native_axes_meta[bt_data->base.pids->id];
    const uint8_t *axes_idx = sw_axes_idx;

    memset((uint8_t *)meta, 0, sizeof(*meta));

    bt_hid_sw_get_calib(bt_data->base.pids->id, &calib);

    memcpy(bt_data->raw_src_mappings[PAD].btns_mask, sw_native_btns_mask,
        sizeof(bt_data->raw_src_mappings[PAD].btns_mask));

    mapping_quirks_apply(bt_data);

    if (bt_data->base.pids->subtype == BT_SW_LEFT_JOYCON) {
        meta[0].polarity = 1;
        meta[1].polarity = 0;
        axes_idx = sw_jc_axes_idx;
        memcpy(bt_data->raw_src_mappings[PAD].mask, sw_jc_mask,
            sizeof(bt_data->raw_src_mappings[PAD].mask));
        memcpy(bt_data->raw_src_mappings[PAD].desc, sw_jc_desc,
            sizeof(bt_data->raw_src_mappings[PAD].desc));
    }
    else if (bt_data->base.pids->subtype == BT_SW_RIGHT_JOYCON) {
        meta[0].polarity = 0;
        meta[1].polarity = 1;
        axes_idx = sw_jc_axes_idx;
        memcpy(bt_data->raw_src_mappings[PAD].mask, sw_jc_mask,
            sizeof(bt_data->raw_src_mappings[PAD].mask));
        memcpy(bt_data->raw_src_mappings[PAD].desc, sw_jc_desc,
            sizeof(bt_data->raw_src_mappings[PAD].desc));
    }
    else if (bt_data->base.pids->subtype == BT_SUBTYPE_DEFAULT || bt_data->base.pids->subtype == BT_SW_POWERA) {
        meta[0].polarity = 0;
        meta[1].polarity = 0;
        axes_idx = sw_axes_idx;
        memcpy(bt_data->raw_src_mappings[PAD].mask, sw_mask,
            sizeof(bt_data->raw_src_mappings[PAD].mask));
        memcpy(bt_data->raw_src_mappings[PAD].desc, sw_desc,
            sizeof(bt_data->raw_src_mappings[PAD].desc));
    }
    else if (bt_data->base.pids->subtype == BT_SW_MD_GEN) {
        memcpy(bt_data->raw_src_mappings[PAD].btns_mask, sw_gen_btns_mask,
            sizeof(bt_data->raw_src_mappings[PAD].btns_mask));

        meta[0].polarity = 0;
        meta[1].polarity = 0;
        axes_idx = sw_axes_idx;
        memcpy(bt_data->raw_src_mappings[PAD].mask, sw_gen_mask,
            sizeof(bt_data->raw_src_mappings[PAD].mask));
        memcpy(bt_data->raw_src_mappings[PAD].desc, sw_gen_desc,
            sizeof(bt_data->raw_src_mappings[PAD].desc));
    }
    else if (bt_data->base.pids->subtype == BT_SW_N64) {
        memcpy(bt_data->raw_src_mappings[PAD].btns_mask, sw_n64_btns_mask,
            sizeof(bt_data->raw_src_mappings[PAD].btns_mask));

        meta[0].polarity = 0;
        meta[1].polarity = 0;
        axes_idx = sw_axes_idx;
        memcpy(bt_data->raw_src_mappings[PAD].mask, sw_n64_mask,
            sizeof(bt_data->raw_src_mappings[PAD].mask));
        memcpy(bt_data->raw_src_mappings[PAD].desc, sw_n64_desc,
            sizeof(bt_data->raw_src_mappings[PAD].desc));
    }
    else {
        meta[0].polarity = 0;
        meta[1].polarity = 0;
        axes_idx = sw_axes_idx;
        memcpy(bt_data->raw_src_mappings[PAD].mask, sw_legacy_mask,
            sizeof(bt_data->raw_src_mappings[PAD].mask));
        memcpy(bt_data->raw_src_mappings[PAD].desc, sw_legacy_desc,
            sizeof(bt_data->raw_src_mappings[PAD].desc));
    }

    for (uint32_t i = 0; i < SW_AXES_MAX; i++) {
        if (calib->sticks[i / 2].axes[i % 2].neutral) {
            meta[axes_idx[i]].neutral = calib->sticks[i / 2].axes[i % 2].neutral;
            meta[axes_idx[i]].abs_max = MIN(calib->sticks[i / 2].axes[i % 2].rel_min, calib->sticks[i / 2].axes[i % 2].rel_max);
            meta[axes_idx[i]].deadzone = calib->sticks[i / 2].deadzone;
        }
        else {
            meta[axes_idx[i]].neutral = sw_native_axes_meta_default[i].neutral;
            meta[axes_idx[i]].abs_max = sw_native_axes_meta_default[i].abs_max;
            meta[axes_idx[i]].deadzone = sw_native_axes_meta_default[i].deadzone;
        }
        bt_data->base.axes_cal[i] = 0;
    }

    atomic_set_bit(&bt_data->base.flags[PAD], BT_INIT);
}

static void sw_native_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    struct sw_native_map *map = (struct sw_native_map *)bt_data->base.input;
    struct ctrl_meta *meta = sw_native_axes_meta[bt_data->base.pids->id];
    uint16_t axes[4];

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        sw_native_pad_init(bt_data);
    }

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)bt_data->raw_src_mappings[PAD].mask;
    ctrl_data->desc = (uint32_t *)bt_data->raw_src_mappings[PAD].desc;

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (map->buttons & bt_data->raw_src_mappings[PAD].btns_mask[i]) {
            ctrl_data->btns[0].value |= generic_btns_mask[i];
        }
    }

    if (bt_data->base.pids->subtype == BT_SW_LEFT_JOYCON) {
        axes[1] = map->axes[0] | ((map->axes[1] & 0xF) << 8);
        axes[0] = (map->axes[1] >> 4) | (map->axes[2] << 4);
        axes[3] = map->axes[3] | ((map->axes[4] & 0xF) << 8);
        axes[2] = (map->axes[4] >> 4) | (map->axes[5] << 4);
    }
    else if (bt_data->base.pids->subtype == BT_SW_RIGHT_JOYCON) {
        axes[1] = map->axes[3] | ((map->axes[4] & 0xF) << 8);
        axes[0] = (map->axes[4] >> 4) | (map->axes[5] << 4);
        axes[3] = map->axes[0] | ((map->axes[1] & 0xF) << 8);
        axes[2] = (map->axes[1] >> 4) | (map->axes[2] << 4);
    }
    else {
        axes[0] = map->axes[0] | ((map->axes[1] & 0xF) << 8);
        axes[1] = (map->axes[1] >> 4) | (map->axes[2] << 4);
        axes[2] = map->axes[3] | ((map->axes[4] & 0xF) << 8);
        axes[3] = (map->axes[4] >> 4) | (map->axes[5] << 4);
    }

    for (uint32_t i = 0; i < SW_AXES_MAX; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = axes[i] - meta[i].neutral + bt_data->base.axes_cal[i];
    }
}

static void sw_hid_pad_init(struct bt_data *bt_data) {
    struct sw_map *map = (struct sw_map *)bt_data->base.input;

    memcpy(bt_data->raw_src_mappings[PAD].mask, sw_mask,
        sizeof(bt_data->raw_src_mappings[PAD].mask));
    memcpy(bt_data->raw_src_mappings[PAD].desc, sw_desc,
        sizeof(bt_data->raw_src_mappings[PAD].desc));
    memcpy(bt_data->raw_src_mappings[PAD].btns_mask, sw_btns_mask,
        sizeof(bt_data->raw_src_mappings[PAD].btns_mask));
    bt_data->raw_src_mappings[PAD].meta = sw_axes_meta;

    mapping_quirks_apply(bt_data);

    if (bt_data->base.pids->subtype == BT_SW_N64) {
        bt_data->raw_src_mappings[PAD].btns_mask[PAD_RX_LEFT] = BIT(SW_X);
        bt_data->raw_src_mappings[PAD].btns_mask[PAD_RX_RIGHT] = BIT(SW_LJ);
        bt_data->raw_src_mappings[PAD].btns_mask[PAD_RY_DOWN] = BIT(SW_Y);
        bt_data->raw_src_mappings[PAD].btns_mask[PAD_RY_UP] = BIT(SW_RJ);
        bt_data->raw_src_mappings[PAD].btns_mask[PAD_LJ] = 0;
        bt_data->raw_src_mappings[PAD].btns_mask[PAD_RJ] = 0;
        bt_data->raw_src_mappings[PAD].btns_mask[PAD_RB_LEFT] = BIT(SW_A);

        memcpy(bt_data->raw_src_mappings[PAD].mask, sw_n64_mask,
            sizeof(bt_data->raw_src_mappings[PAD].mask));
        memcpy(bt_data->raw_src_mappings[PAD].desc, sw_n64_desc,
            sizeof(bt_data->raw_src_mappings[PAD].desc));
        bt_data->raw_src_mappings[PAD].meta = sw_3rd_axes_meta;
    }
    else if (bt_data->base.pids->subtype == BT_SW_HYPERKIN_ADMIRAL) {
        memcpy(bt_data->raw_src_mappings[PAD].btns_mask, sw_admiral_btns_mask,
            sizeof(bt_data->raw_src_mappings[PAD].btns_mask));

        memcpy(bt_data->raw_src_mappings[PAD].mask, sw_n64_mask,
            sizeof(bt_data->raw_src_mappings[PAD].mask));
        memcpy(bt_data->raw_src_mappings[PAD].desc, sw_n64_desc,
            sizeof(bt_data->raw_src_mappings[PAD].desc));
        bt_data->raw_src_mappings[PAD].meta = sw_3rd_axes_meta;
    }

    for (uint32_t i = 0; i < SW_AXES_MAX; i++) {
        bt_data->base.axes_cal[i] = -(map->axes[sw_axes_idx[i]] - sw_axes_meta[i].neutral);
    }

    atomic_set_bit(&bt_data->base.flags[PAD], BT_INIT);
}

static void sw_hid_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    struct sw_map *map = (struct sw_map *)bt_data->base.input;

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        sw_hid_pad_init(bt_data);
    }

    const struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)bt_data->raw_src_mappings[PAD].mask;
    ctrl_data->desc = (uint32_t *)bt_data->raw_src_mappings[PAD].desc;

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (map->buttons & bt_data->raw_src_mappings[PAD].btns_mask[i]) {
            ctrl_data->btns[0].value |= generic_btns_mask[i];
        }
    }

    /* Convert hat to regular btns */
    if (bt_data->base.pids->subtype == BT_SW_HYPERKIN_ADMIRAL) {
        ctrl_data->btns[0].value |= hat_to_ld_btns[(map->hat - 1) & 0xF];
    }
    else {
        ctrl_data->btns[0].value |= hat_to_ld_btns[map->hat & 0xF];
    }

    for (uint32_t i = 0; i < SW_AXES_MAX; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = map->axes[sw_axes_idx[i]] - meta[i].neutral + bt_data->base.axes_cal[i];
    }
}

int32_t sw_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    if (bt_data->base.report_id == 0x30) {
        sw_native_to_generic(bt_data, ctrl_data);
    }
    else if (bt_data->base.report_id == 0x3F) {
        sw_hid_to_generic(bt_data, ctrl_data);
    }
    else {
        return -1;
    }
    return 0;
}

void sw_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data) {
    struct sw_conf *conf = (struct sw_conf *)bt_data->base.output;
    memset((void *)conf, 0, sizeof(*conf));

    conf->subcmd = BT_HIDP_SW_SUBCMD_SET_LED;
    conf->subcmd_data[0] = led_dev_id_map[bt_data->base.pids->id];

    if (fb_data->state) {
        memcpy((void *)conf->rumble, (void *)sw_rumble_on, sizeof(sw_rumble_on));
    }
    else {
        memcpy((void *)conf->rumble, (void *)sw_rumble_off, sizeof(sw_rumble_off));
    }
}
