/*
 * Copyright (c) 2019-2023, Jacques Gagnon
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
    SW_PRO_B = 0,
    SW_PRO_A,
    SW_PRO_Y,
    SW_PRO_X,
    SW_PRO_L,
    SW_PRO_R,
    SW_PRO_ZL,
    SW_PRO_ZR,
    SW_PRO_MINUS,
    SW_PRO_PLUS,
    SW_PRO_LJ,
    SW_PRO_RJ,
    SW_PRO_HOME,
    SW_PRO_CAPTURE,
    SW_SNES_ZR = 15,
};

enum {
    SW_LJC_DOWN = 0,
    SW_LJC_RIGHT,
    SW_LJC_LEFT,
    SW_LJC_UP,
    SW_LJC_SL,
    SW_LJC_SR,
    SW_LJC_MINUS = 8,
    SW_LJC_LJ = 10,
    SW_LJC_CAPTURE = 13,
    SW_LJC_L,
    SW_LJC_ZL,
};

enum {
    SW_RJC_A = 0,
    SW_RJC_X,
    SW_RJC_B,
    SW_RJC_Y,
    SW_RJC_SL,
    SW_RJC_SR,
    SW_RJC_PLUS = 9,
    SW_RJC_RJ = 11,
    SW_RJC_HOME,
    SW_RJC_R = 14,
    SW_RJC_ZR,
};

enum {
    SW_GEN_B = 0,
    SW_GEN_A,
    SW_GEN_Y,
    SW_GEN_Z = 4,
    SW_GEN_C,
    SW_GEN_X,
    SW_GEN_MODE,
    SW_GEN_START = 9,
    SW_GEN_HOME = 12,
    SW_GEN_CAPTURE,
};

enum {
    SW_N64_B = 0,
    SW_N64_A,
    SW_N64_C_UP,
    SW_N64_C_LEFT,
    SW_N64_L,
    SW_N64_R,
    SW_N64_Z,
    SW_N64_C_DOWN,
    SW_N64_C_RIGHT,
    SW_N64_START,
    SW_N64_ZR,
    SW_N64_HOME = 12,
    SW_N64_CAPTURE,
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
    {.neutral = 0x800, .abs_max = 0x578, .abs_min = 0x578},
    {.neutral = 0x800, .abs_max = 0x578, .abs_min = 0x578},
    {.neutral = 0x800, .abs_max = 0x578, .abs_min = 0x578},
    {.neutral = 0x800, .abs_max = 0x578, .abs_min = 0x578},
};

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

static const uint32_t sw_pro_mask[4] = {0xFFFF0FFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_pro_desc[4] = {0x000000FF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_legacy_mask[4] = {0xFFFF0F00, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_legacy_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_pro_btns_mask[2][32] = {
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(SW_PRO_Y), BIT(SW_PRO_A), BIT(SW_PRO_B), BIT(SW_PRO_X),
        BIT(SW_PRO_PLUS), BIT(SW_PRO_MINUS), BIT(SW_PRO_HOME), BIT(SW_PRO_CAPTURE),
        BIT(SW_PRO_ZL), BIT(SW_PRO_L), 0, BIT(SW_PRO_LJ),
        BIT(SW_PRO_ZR) | BIT(SW_SNES_ZR), BIT(SW_PRO_R), 0, BIT(SW_PRO_RJ),
    },
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(SW_N_LEFT), BIT(SW_N_RIGHT), BIT(SW_N_DOWN), BIT(SW_N_UP),
        0, 0, 0, 0,
        BIT(SW_N_Y), BIT(SW_N_A), BIT(SW_N_B), BIT(SW_N_X),
        BIT(SW_N_PLUS), BIT(SW_N_MINUS), BIT(SW_N_HOME), BIT(SW_N_CAPTURE),
        BIT(SW_N_ZL), BIT(SW_N_L), BIT(SW_N_L_SL) | BIT(SW_N_R_SL), BIT(SW_N_LJ),
        BIT(SW_N_ZR), BIT(SW_N_R), BIT(SW_N_L_SR) | BIT(SW_N_R_SR), BIT(SW_N_RJ),
    },
};

static const uint32_t sw_jc_mask[4] = {0x3B3F000F, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_jc_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_jc_native_desc[4] = {0x0000000F, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_jc_btns_mask[2][32] = {
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(SW_LJC_LEFT), BIT(SW_LJC_RIGHT), BIT(SW_LJC_DOWN), BIT(SW_LJC_UP),
        BIT(SW_LJC_CAPTURE) | BIT(SW_RJC_PLUS), BIT(SW_LJC_MINUS) | BIT(SW_RJC_HOME), 0, 0,
        BIT(SW_LJC_SL), BIT(SW_LJC_L) | BIT(SW_RJC_R), 0, BIT(SW_LJC_LJ) | BIT(SW_RJC_RJ),
        BIT(SW_LJC_SR), BIT(SW_LJC_ZL) | BIT(SW_RJC_ZR), 0, 0,
    },
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(SW_N_B) | BIT(SW_N_UP), BIT(SW_N_X) | BIT(SW_N_DOWN), BIT(SW_N_A) | BIT(SW_N_LEFT), BIT(SW_N_Y) | BIT(SW_N_RIGHT),
        BIT(SW_N_CAPTURE) | BIT(SW_N_PLUS), BIT(SW_N_MINUS) | BIT(SW_N_HOME), 0, 0,
        BIT(SW_N_L_SL) | BIT(SW_N_R_SL), BIT(SW_N_ZL) | BIT(SW_N_ZR), 0, BIT(SW_N_LJ) | BIT(SW_N_RJ),
        BIT(SW_N_L_SR) | BIT(SW_N_R_SR), BIT(SW_N_L) | BIT(SW_N_R), 0, 0,
    },
};

static const uint32_t sw_gen_mask[4] = {0x22FF0F00, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_gen_desc[4] = {0x0000000F, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_gen_btns_mask[2][32] = {
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(SW_GEN_A), BIT(SW_GEN_C), BIT(SW_GEN_B), BIT(SW_GEN_Y),
        BIT(SW_GEN_START), BIT(SW_GEN_MODE), BIT(SW_GEN_HOME), BIT(SW_GEN_CAPTURE),
        0, BIT(SW_GEN_X), 0, 0,
        0, BIT(SW_GEN_Z), 0, 0,
    },
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(SW_N_LEFT), BIT(SW_N_RIGHT), BIT(SW_N_DOWN), BIT(SW_N_UP),
        0, 0, 0, 0,
        BIT(SW_N_A), BIT(SW_N_R), BIT(SW_N_B), BIT(SW_N_Y),
        BIT(SW_N_PLUS), BIT(SW_N_ZR), BIT(SW_N_HOME), BIT(SW_N_CAPTURE),
        0, BIT(SW_N_X), 0, 0,
        0, BIT(SW_N_L), 0, 0,
    },
};

static const uint32_t sw_n64_mask[4] = {0x33D50FFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_n64_desc[4] = {0x0000000F, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw_n64_btns_mask[2][32] = {
    {
        0, 0, 0, 0,
        BIT(SW_N64_C_LEFT), BIT(SW_N64_C_RIGHT), BIT(SW_N64_C_DOWN), BIT(SW_N64_C_UP),
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(SW_N64_B), 0, BIT(SW_N64_A), 0,
        BIT(SW_N64_START), 0, BIT(SW_N64_HOME), BIT(SW_N64_CAPTURE),
        BIT(SW_N64_Z), BIT(SW_N64_L), 0, 0,
        BIT(SW_N64_ZR), BIT(SW_N64_R), 0, 0,
    },
    {
        0, 0, 0, 0,
        BIT(SW_N_X), BIT(SW_N_MINUS), BIT(SW_N_ZR), BIT(SW_N_Y),
        BIT(SW_N_LEFT), BIT(SW_N_RIGHT), BIT(SW_N_DOWN), BIT(SW_N_UP),
        0, 0, 0, 0,
        BIT(SW_N_B), 0, BIT(SW_N_A), 0,
        BIT(SW_N_PLUS), 0, BIT(SW_N_HOME), BIT(SW_N_CAPTURE),
        BIT(SW_N_ZL), BIT(SW_N_L), 0, 0,
        BIT(SW_N_LJ), BIT(SW_N_R), 0, 0,
    },
};

static const uint32_t sw_admiral_btns_mask[32] = {
    0, 0, 0, 0,
    BIT(SW_PRO_MINUS), BIT(SW_PRO_RJ), BIT(SW_PRO_CAPTURE), BIT(SW_PRO_HOME),
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(SW_PRO_B), 0, BIT(SW_PRO_A), 0,
    BIT(SW_PRO_PLUS), 0, 0, 0,
    BIT(SW_LJC_L), BIT(SW_PRO_L), 0, 0,
    0, BIT(SW_PRO_R), 0, 0,
};

static const uint32_t sw_brawler64_btns_mask[32] = {
    0, 0, 0, 0,
    BIT(SW_PRO_X), BIT(SW_PRO_LJ), BIT(SW_PRO_Y), BIT(SW_PRO_RJ),
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(SW_PRO_A), 0, BIT(SW_PRO_B), 0,
    BIT(SW_PRO_PLUS), BIT(SW_PRO_MINUS), BIT(SW_PRO_HOME), BIT(SW_PRO_CAPTURE),
    BIT(SW_PRO_ZL), BIT(SW_PRO_L), 0, 0,
    BIT(SW_PRO_ZR), BIT(SW_PRO_R), 0, 0,
};

static int32_t sw_pad_init(struct bt_data *bt_data) {
    struct bt_hid_sw_ctrl_calib *calib = NULL;
    const uint8_t *axes_idx = sw_axes_idx;
    struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;
    uint8_t report_type = (bt_data->base.report_id == 0x30) ? 1 : 0;

    memcpy(bt_data->raw_src_mappings[PAD].btns_mask, &sw_pro_btns_mask[report_type],
        sizeof(bt_data->raw_src_mappings[PAD].btns_mask));

    mapping_quirks_apply(bt_data);

    bt_hid_sw_get_calib(bt_data->base.pids->id, &calib);

    if (bt_data->base.pids->subtype == BT_SW_LEFT_JOYCON) {
        const uint32_t *desc = (report_type) ? sw_jc_native_desc : sw_jc_desc;
        memcpy(bt_data->raw_src_mappings[PAD].btns_mask, &sw_jc_btns_mask[report_type],
            sizeof(bt_data->raw_src_mappings[PAD].btns_mask));

        meta[0].polarity = 1;
        meta[1].polarity = 0;
        axes_idx = sw_jc_axes_idx;
        memcpy(bt_data->raw_src_mappings[PAD].mask, sw_jc_mask,
            sizeof(bt_data->raw_src_mappings[PAD].mask));
        memcpy(bt_data->raw_src_mappings[PAD].desc, desc,
            sizeof(bt_data->raw_src_mappings[PAD].desc));
    }
    else if (bt_data->base.pids->subtype == BT_SW_RIGHT_JOYCON) {
        const uint32_t *desc = (report_type) ? sw_jc_native_desc : sw_jc_desc;
        memcpy(bt_data->raw_src_mappings[PAD].btns_mask, &sw_jc_btns_mask[report_type],
            sizeof(bt_data->raw_src_mappings[PAD].btns_mask));

        meta[0].polarity = 0;
        meta[1].polarity = 1;
        axes_idx = sw_jc_axes_idx;
        memcpy(bt_data->raw_src_mappings[PAD].mask, sw_jc_mask,
            sizeof(bt_data->raw_src_mappings[PAD].mask));
        memcpy(bt_data->raw_src_mappings[PAD].desc, desc,
            sizeof(bt_data->raw_src_mappings[PAD].desc));
    }
    else if (bt_data->base.pids->subtype == BT_SUBTYPE_DEFAULT || bt_data->base.pids->subtype == BT_SW_POWERA) {
        meta[0].polarity = 0;
        meta[1].polarity = 0;
        meta[2].polarity = 0;
        meta[3].polarity = 0;
        axes_idx = sw_axes_idx;
        memcpy(bt_data->raw_src_mappings[PAD].mask, sw_pro_mask,
            sizeof(bt_data->raw_src_mappings[PAD].mask));
        memcpy(bt_data->raw_src_mappings[PAD].desc, sw_pro_desc,
            sizeof(bt_data->raw_src_mappings[PAD].desc));
    }
    else if (bt_data->base.pids->subtype == BT_SW_MD_GEN) {
        memcpy(bt_data->raw_src_mappings[PAD].btns_mask, &sw_gen_btns_mask[report_type],
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
        if (bt_data->base.report_id == 0x3F && calib == NULL) {
            /* RF Brawler64 HID mapping is wrong, if no calib data provided */
            /* while using the N64 name we assume it's the RF Brawler64 */
            memcpy(bt_data->raw_src_mappings[PAD].btns_mask, &sw_brawler64_btns_mask,
                sizeof(bt_data->raw_src_mappings[PAD].btns_mask));
        }
        else {
            memcpy(bt_data->raw_src_mappings[PAD].btns_mask, &sw_n64_btns_mask[report_type],
                sizeof(bt_data->raw_src_mappings[PAD].btns_mask));
        }

        meta[0].polarity = 0;
        meta[1].polarity = 0;
        axes_idx = sw_axes_idx;
        memcpy(bt_data->raw_src_mappings[PAD].mask, sw_n64_mask,
            sizeof(bt_data->raw_src_mappings[PAD].mask));
        memcpy(bt_data->raw_src_mappings[PAD].desc, sw_n64_desc,
            sizeof(bt_data->raw_src_mappings[PAD].desc));
    }
    else if (bt_data->base.pids->subtype == BT_SW_HYPERKIN_ADMIRAL) {
        memcpy(bt_data->raw_src_mappings[PAD].btns_mask, sw_admiral_btns_mask,
            sizeof(bt_data->raw_src_mappings[PAD].btns_mask));

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
        if (calib && calib->sticks[i / 2].axes[i % 2].neutral) {
            meta[axes_idx[i]].neutral = calib->sticks[i / 2].axes[i % 2].neutral;
            meta[axes_idx[i]].abs_max = calib->sticks[i / 2].axes[i % 2].rel_max * MAX_PULL_BACK;
            meta[axes_idx[i]].abs_min = calib->sticks[i / 2].axes[i % 2].rel_min * MAX_PULL_BACK;
            meta[axes_idx[i]].deadzone = calib->sticks[i / 2].deadzone;
            printf("# %s: controller calib loaded\n", __FUNCTION__);
        }
        else {
            meta[axes_idx[i]].neutral = sw_axes_meta[i].neutral;
            meta[axes_idx[i]].abs_max = sw_axes_meta[i].abs_max * MAX_PULL_BACK;
            meta[axes_idx[i]].abs_min = sw_axes_meta[i].abs_min * MAX_PULL_BACK;
            meta[axes_idx[i]].deadzone = sw_axes_meta[i].deadzone;
            printf("# %s: no calib, using default\n", __FUNCTION__);
        }
        bt_data->base.axes_cal[i] = 0;
    }

    atomic_set_bit(&bt_data->base.flags[PAD], BT_INIT);
    return 0;
}

static int32_t sw_native_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    struct sw_native_map *map = (struct sw_native_map *)bt_data->base.input;
    struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;
    uint16_t axes[4];

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        if (sw_pad_init(bt_data)) {
            return -1;
        }
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
        axes[3] = map->axes[0] | ((map->axes[1] & 0xF) << 8);
        axes[2] = (map->axes[1] >> 4) | (map->axes[2] << 4);
        axes[1] = map->axes[3] | ((map->axes[4] & 0xF) << 8);
        axes[0] = (map->axes[4] >> 4) | (map->axes[5] << 4);
    }
    else {
        axes[0] = map->axes[0] | ((map->axes[1] & 0xF) << 8);
        axes[1] = (map->axes[1] >> 4) | (map->axes[2] << 4);
        axes[2] = map->axes[3] | ((map->axes[4] & 0xF) << 8);
        axes[3] = (map->axes[4] >> 4) | (map->axes[5] << 4);
    }

#ifdef CONFIG_BLUERETRO_RAW_INPUT
    printf("{\"log_type\": \"wireless_input\", \"axes\": [%u, %u, %u, %u], \"btns\": %lu}\n",
        axes[0], axes[1], axes[2], axes[3], map->buttons);
#endif

    for (uint32_t i = 0; i < SW_AXES_MAX; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = axes[i] - meta[i].neutral;
    }
    return 0;
}

static int32_t sw_hid_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    struct sw_map *map = (struct sw_map *)bt_data->base.input;
    struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;

#ifdef CONFIG_BLUERETRO_RAW_INPUT
    printf("{\"log_type\": \"wireless_input\", \"report_id\": %ld, \"axes\": [%u, %u, %u, %u], \"btns\": %u, \"hat\": %u}\n",
        bt_data->base.report_id, map->axes[sw_axes_idx[0]], map->axes[sw_axes_idx[1]],
        map->axes[sw_axes_idx[2]], map->axes[sw_axes_idx[3]], map->buttons, map->hat);
#endif

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        if (sw_pad_init(bt_data)) {
            return -1;
        }
    }

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)bt_data->raw_src_mappings[PAD].mask;
    ctrl_data->desc = (uint32_t *)bt_data->raw_src_mappings[PAD].desc;

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (map->buttons & bt_data->raw_src_mappings[PAD].btns_mask[i]) {
            ctrl_data->btns[0].value |= generic_btns_mask[i];
        }
    }

    if (bt_data->base.pids->subtype == BT_SW_LEFT_JOYCON || bt_data->base.pids->subtype == BT_SW_RIGHT_JOYCON) {
        uint32_t hat = hat_to_ld_btns[map->hat & 0xF];
        ctrl_data->btns[0].value |= hat >> 8;
    }
    else {
        /* Convert hat to regular btns */
        if (bt_data->base.pids->subtype == BT_SW_HYPERKIN_ADMIRAL) {
            ctrl_data->btns[0].value |= hat_to_ld_btns[(map->hat - 1) & 0xF];
        }
        else {
            ctrl_data->btns[0].value |= hat_to_ld_btns[map->hat & 0xF];
        }

        for (uint32_t i = 0; i < SW_AXES_MAX; i++) {
            ctrl_data->axes[i].meta = &meta[i];
            if (i & 0x1) {
                ctrl_data->axes[i].value = (~(map->axes[sw_axes_idx[i]] >> 4) & 0xFFF) + 1 - meta[i].neutral;
            }
            else {
                ctrl_data->axes[i].value = (map->axes[sw_axes_idx[i]] >> 4) - meta[i].neutral;
            }
        }
    }
    return 0;
}

int32_t sw_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    if (bt_data->base.report_id == 0x30) {
        return sw_native_to_generic(bt_data, ctrl_data);
    }
    else if (bt_data->base.report_id == 0x3F) {
        return sw_hid_to_generic(bt_data, ctrl_data);
    }
    return -1;
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

