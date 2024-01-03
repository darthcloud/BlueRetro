/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "ps3.h"

enum {
    PS3_SELECT = 8,
    PS3_L3,
    PS3_R3,
    PS3_START,
    PS3_D_UP,
    PS3_D_RIGHT,
    PS3_D_DOWN,
    PS3_D_LEFT,
    PS3_L2,
    PS3_R2,
    PS3_L1,
    PS3_R1,
    PS3_T,
    PS3_C,
    PS3_X,
    PS3_S,
    PS3_PS,
};

static const uint8_t led_dev_id_map[] = {
    0x1, 0x2, 0x4, 0x8, 0x3, 0x6, 0xC
};

static const uint8_t ps3_config[] = {
    0x01, 0xfe, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0xff, 0x27, 0x10, 0x00, 0x32, 0xff,
    0x27, 0x10, 0x00, 0x32, 0xff, 0x27, 0x10, 0x00,
    0x32, 0xff, 0x27, 0x10, 0x00, 0x32, 0x00, 0x00,
    0x00, 0x00, 0x00
};

static const uint8_t ps3_axes_idx[ADAPTER_PS2_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       2,       3,       12,     13,
/*  TRIG_LS, TRIG_RS, DPAD_L,  DPAD_R,  DPAD_D, DPAD_U  */
    14,      15,      11,      9,       10,     8,
/*  BTN_L,   BTN_R,   BTN_D,   BTN_U  */
    19,      17,      18,      16
};

static const struct ctrl_meta ps3_axes_meta[ADAPTER_PS2_MAX_AXES] =
{
    {.neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80},
    {.neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80, .polarity = 1},
    {.neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80},
    {.neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80, .polarity = 1},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
};

struct ps3_map {
    uint32_t buttons;
    uint8_t reserved;
    uint8_t axes[20];
} __packed;

struct ps3_set_conf {
    uint8_t tbd0;
    uint8_t r_rumble_len;
    uint8_t r_rumble_pow;
    uint8_t l_rumble_len;
    uint8_t l_rumble_pow;
    uint8_t tbd1[4];
    uint8_t leds;
    uint8_t tbd2[25];
} __packed;

static const uint32_t ps3_mask[4] = {0xBB7F0FFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t ps3_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t ps3_pressure_desc[4] = {0x330F0FFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t ps3_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(PS3_D_LEFT), BIT(PS3_D_RIGHT), BIT(PS3_D_DOWN), BIT(PS3_D_UP),
    0, 0, 0, 0,
    BIT(PS3_S), BIT(PS3_C), BIT(PS3_X), BIT(PS3_T),
    BIT(PS3_START), BIT(PS3_SELECT), BIT(PS3_PS), 0,
    0, BIT(PS3_L1), 0, BIT(PS3_L3),
    0, BIT(PS3_R1), 0, BIT(PS3_R3),
};

int32_t ps3_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    uint32_t i = 8;
    uint32_t axes_cnt = ADAPTER_MAX_AXES;
    struct ps3_map *map = (struct ps3_map *)bt_data->base.input;
    struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;

#ifdef CONFIG_BLUERETRO_RAW_INPUT
    printf("{\"log_type\": \"wireless_input\", \"report_id\": %ld, \"axes\": [%u, %u, %u, %u, %u, %u], \"btns\": %lu}\n",
        bt_data->base.report_id, map->axes[ps3_axes_idx[0]], map->axes[ps3_axes_idx[1]], map->axes[ps3_axes_idx[2]],
        map->axes[ps3_axes_idx[3]], map->axes[ps3_axes_idx[4]], map->axes[ps3_axes_idx[5]], map->buttons);
#endif

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)ps3_mask;
    ctrl_data->desc = (uint32_t *)ps3_desc;

    if (wired_adapter.system_id == PSX || wired_adapter.system_id == PS2) {
        ctrl_data->desc = (uint32_t *)ps3_pressure_desc;
        axes_cnt = ADAPTER_PS2_MAX_AXES;
        i = 20;
    }

    for (; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (map->buttons & ps3_btns_mask[i]) {
            ctrl_data->btns[0].value |= generic_btns_mask[i];
        }
    }

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        memcpy(meta, ps3_axes_meta, sizeof(ps3_axes_meta));
        for (uint32_t i = 0; i < ADAPTER_PS2_MAX_AXES; i++) {
            meta[i].abs_max *= MAX_PULL_BACK;
            meta[i].abs_min *= MAX_PULL_BACK;
            bt_data->base.axes_cal[i] = -(map->axes[ps3_axes_idx[i]] - ps3_axes_meta[i].neutral);
        }
        atomic_set_bit(&bt_data->base.flags[PAD], BT_INIT);
    }

    for (i = 0; i < axes_cnt; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = map->axes[ps3_axes_idx[i]] - ps3_axes_meta[i].neutral + bt_data->base.axes_cal[i];
    }

    return 0;
}

void ps3_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data) {
    struct ps3_set_conf *set_conf = (struct ps3_set_conf *)bt_data->base.output;
    memcpy((void *)set_conf, ps3_config, sizeof(*set_conf));
    set_conf->leds = (led_dev_id_map[bt_data->base.pids->id] << 1);

    if (fb_data->state) {
        set_conf->r_rumble_pow = 0x01;
    }
}
