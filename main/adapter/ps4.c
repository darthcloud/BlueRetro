/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "../zephyr/types.h"
#include "../util.h"
#include "ps4.h"

enum {
    PS4_S = 4,
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

static const uint8_t ps4_led_dev_id_map[][3] = {
    {0x00, 0x00, 0x40},
    {0x40, 0x00, 0x00},
    {0x00, 0x40, 0x00},
    {0x20, 0x00, 0x20},
    {0x02, 0x01, 0x00},
    {0x00, 0x01, 0x01},
    {0x01, 0x01, 0x01},
};

const uint8_t ps4_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       2,       3,       7,      8
};

const struct ctrl_meta ps4_btn_meta =
{
    .polarity = 0,
};

const struct ctrl_meta ps4_axes_meta[ADAPTER_MAX_AXES] =
{
    {.neutral = 0x80, .abs_max = 0x80},
    {.neutral = 0x80, .abs_max = 0x80, .polarity = 1},
    {.neutral = 0x80, .abs_max = 0x80},
    {.neutral = 0x80, .abs_max = 0x80, .polarity = 1},
    {.neutral = 0x00, .abs_max = 0xFF},
    {.neutral = 0x00, .abs_max = 0xFF},
};

struct hid_map {
    union {
        struct {
            uint8_t reserved2[4];
            union {
                uint8_t hat;
                uint32_t buttons;
            };
        };
        uint8_t axes[9];
    };
} __packed;

struct ps4_map {
    uint8_t reserved[2];
    union {
        struct {
            uint8_t reserved2[4];
            union {
                uint8_t hat;
                uint32_t buttons;
            };
        };
        uint8_t axes[9];
    };
} __packed;

struct ps4_set_conf {
    uint8_t conf0;
    uint8_t tbd0;
    uint8_t conf1;
    uint8_t tbd1[2];
    uint8_t r_rumble;
    uint8_t l_rumble;
    uint8_t leds[3];
    uint8_t led_on_delay;
    uint8_t led_off_delay;
    uint8_t tbd2[61];
    uint32_t crc;
} __packed;

const uint32_t ps4_mask[4] = {0xBBFF0FFF, 0x00000000, 0x00000000, 0x00000000};
const uint32_t ps4_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};

const uint32_t ps4_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(PS4_S), BIT(PS4_C), BIT(PS4_X), BIT(PS4_T),
    BIT(PS4_OPTIONS), BIT(PS4_SHARE), BIT(PS4_PS), BIT(PS4_TP),
    0, BIT(PS4_L1), 0, BIT(PS4_L3),
    0, BIT(PS4_R1), 0, BIT(PS4_R3),
};

void ps_ps4_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    struct ps4_map *map = (struct ps4_map *)bt_data->input;

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)ps4_mask;
    ctrl_data->desc = (uint32_t *)ps4_desc;

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (map->buttons & ps4_btns_mask[i]) {
            ctrl_data->btns[0].value |= generic_btns_mask[i];
        }
    }

    /* Convert hat to regular btns */
    ctrl_data->btns[0].value |= hat_to_ld_btns[map->hat & 0xF];

    if (!atomic_test_bit(&bt_data->flags, BT_INIT)) {
        for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
            bt_data->axes_cal[i] = -(map->axes[ps4_axes_idx[i]] - ps4_axes_meta[i].neutral);
        }
        atomic_set_bit(&bt_data->flags, BT_INIT);
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        ctrl_data->axes[i].meta = &ps4_axes_meta[i];
        ctrl_data->axes[i].value = map->axes[ps4_axes_idx[i]] - ps4_axes_meta[i].neutral + bt_data->axes_cal[i];
    }
}

void ps_hid_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    struct hid_map *map = (struct hid_map *)bt_data->input;

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)ps4_mask;
    ctrl_data->desc = (uint32_t *)ps4_desc;

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (map->buttons & ps4_btns_mask[i]) {
            ctrl_data->btns[0].value |= generic_btns_mask[i];
        }
    }

    /* Convert hat to regular btns */
    ctrl_data->btns[0].value |= hat_to_ld_btns[map->hat & 0xF];

    if (!atomic_test_bit(&bt_data->flags, BT_INIT)) {
        for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
            bt_data->axes_cal[i] = -(map->axes[ps4_axes_idx[i]] - ps4_axes_meta[i].neutral);
        }
        atomic_set_bit(&bt_data->flags, BT_INIT);
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        ctrl_data->axes[i].meta = &ps4_axes_meta[i];
        ctrl_data->axes[i].value = map->axes[ps4_axes_idx[i]] - ps4_axes_meta[i].neutral + bt_data->axes_cal[i];
    }
}

void ps4_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data) {
    switch (bt_data->report_type) {
        case KB:
            ps_hid_to_generic(bt_data, ctrl_data);
            break;
        case 11:
            ps_ps4_to_generic(bt_data, ctrl_data);
            break;
        default:
            printf("# Unknown report type: %02X\n", bt_data->report_type);
            break;
    }
}

void ps4_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data) {
    struct ps4_set_conf *set_conf = (struct ps4_set_conf *)bt_data->output;
    memset((void *)set_conf, 0, sizeof(*set_conf));
    set_conf->conf0 = 0xc4;
    set_conf->conf1 = 0x07;

    memcpy(set_conf->leds, ps4_led_dev_id_map[bt_data->dev_id], sizeof(set_conf->leds));

    if (fb_data->state) {
        set_conf->r_rumble = 0xFF;
    }
}
