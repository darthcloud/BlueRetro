/*
 * Copyright (c) 2019-2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "bluetooth/hidp/ps.h"
#include "adapter/config.h"
#include "tests/cmds.h"
#include "bluetooth/mon.h"
#include "ps.h"

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
    PS5_MUTE,
};

static const uint8_t ps4_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       2,       3,       7,      8
};

static const uint8_t ps5_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       2,       3,       4,      5
};

static const struct ctrl_meta ps4_axes_meta[ADAPTER_MAX_AXES] =
{
    {.neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80},
    {.neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80, .polarity = 1},
    {.neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80},
    {.neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80, .polarity = 1},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
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

struct ps5_map {
    uint8_t reserved;
    uint8_t axes[6];
    uint8_t reserved2;
    union {
        uint8_t hat;
        uint32_t buttons;
    };
} __packed;

static const uint32_t ps4_mask[4] = {0xBBFF0FFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t ps4_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t ps4_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(PS4_S), BIT(PS4_C), BIT(PS4_X), BIT(PS4_T),
    BIT(PS4_OPTIONS), BIT(PS4_SHARE), BIT(PS4_PS), BIT(PS4_TP),
    0, BIT(PS4_L1), 0, BIT(PS4_L3),
    0, BIT(PS4_R1), 0, BIT(PS4_R3),
};

static void ps4_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    struct ps4_map *map = (struct ps4_map *)bt_data->base.input;
    struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;

    TESTS_CMDS_LOG("\"wireless_input\": {\"report_id\": %ld, \"axes\": [%u, %u, %u, %u, %u, %u], \"btns\": %lu, \"hat\": %u},\n",
        bt_data->base.report_id, map->axes[ps4_axes_idx[0]], map->axes[ps4_axes_idx[1]], map->axes[ps4_axes_idx[2]],
        map->axes[ps4_axes_idx[3]], map->axes[ps4_axes_idx[4]], map->axes[ps4_axes_idx[5]], map->buttons, map->hat & 0xF);

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

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        memcpy(meta, ps4_axes_meta, sizeof(ps4_axes_meta));
        bt_mon_log(false, "%s: axes_cal: [", __FUNCTION__);
        for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
            meta[i].abs_max *= MAX_PULL_BACK;
            meta[i].abs_min *= MAX_PULL_BACK;
            bt_data->base.axes_cal[i] = -(map->axes[ps4_axes_idx[i]] - ps4_axes_meta[i].neutral);
            if (i) {
                bt_mon_log(false, ", ");
            }
            bt_mon_log(false, "%d", bt_data->base.axes_cal[i]);
        }
        atomic_set_bit(&bt_data->base.flags[PAD], BT_INIT);
        bt_mon_log(true, "]");
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = map->axes[ps4_axes_idx[i]] - ps4_axes_meta[i].neutral + bt_data->base.axes_cal[i];
    }
}

static void ps5_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    struct ps5_map *map = (struct ps5_map *)bt_data->base.input;
    struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;

    TESTS_CMDS_LOG("\"wireless_input\": {\"report_id\": %ld, \"axes\": [%u, %u, %u, %u, %u, %u], \"btns\": %lu, \"hat\": %u},\n",
        bt_data->base.report_id, map->axes[ps5_axes_idx[0]], map->axes[ps5_axes_idx[1]], map->axes[ps5_axes_idx[2]],
        map->axes[ps5_axes_idx[3]], map->axes[ps5_axes_idx[4]], map->axes[ps5_axes_idx[5]], map->buttons, map->hat & 0xF);

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

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        memcpy(meta, ps4_axes_meta, sizeof(ps4_axes_meta));
        for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
            meta[i].abs_max *= MAX_PULL_BACK;
            meta[i].abs_min *= MAX_PULL_BACK;
            bt_data->base.axes_cal[i] = -(map->axes[ps5_axes_idx[i]] - ps4_axes_meta[i].neutral);
        }
        atomic_set_bit(&bt_data->base.flags[PAD], BT_INIT);
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = map->axes[ps5_axes_idx[i]] - ps4_axes_meta[i].neutral + bt_data->base.axes_cal[i];
    }
}

static void hid_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    struct hid_map *map = (struct hid_map *)bt_data->base.input;
    struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;

    TESTS_CMDS_LOG("\"wireless_input\": {\"report_id\": %ld, \"axes\": [%u, %u, %u, %u, %u, %u], \"btns\": %lu, \"hat\": %u},\n",
        bt_data->base.report_id, map->axes[ps4_axes_idx[0]], map->axes[ps4_axes_idx[1]], map->axes[ps4_axes_idx[2]],
        map->axes[ps4_axes_idx[3]], map->axes[ps4_axes_idx[4]], map->axes[ps4_axes_idx[5]], map->buttons, map->hat & 0xF);

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

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        memcpy(meta, ps4_axes_meta, sizeof(ps4_axes_meta));
        for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
            meta[i].abs_max *= MAX_PULL_BACK;
            meta[i].abs_min *= MAX_PULL_BACK;
            bt_data->base.axes_cal[i] = -(map->axes[ps4_axes_idx[i]] - ps4_axes_meta[i].neutral);
        }
        atomic_set_bit(&bt_data->base.flags[PAD], BT_INIT);
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = map->axes[ps4_axes_idx[i]] - ps4_axes_meta[i].neutral + bt_data->base.axes_cal[i];
    }
}

static void ps4_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data) {
    struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_data->base.output;

    switch (fb_data->type) {
        case FB_TYPE_RUMBLE:
            if (fb_data->state) {
                set_conf->hf_motor_pwr = fb_data->hf_pwr;
                set_conf->lf_motor_pwr = fb_data->lf_pwr;
            }
            else {
                set_conf->hf_motor_pwr = 0x00;
                set_conf->lf_motor_pwr = 0x00;
            }
            break;
        case FB_TYPE_PLAYER_LED:
            set_conf->leds = hw_config.ps_ctrl_colors[bt_data->base.pids->out_idx];
            break;
    }
}

static void ps5_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data) {
    struct bt_hidp_ps5_set_conf *set_conf = (struct bt_hidp_ps5_set_conf *)bt_data->base.output;

    switch (fb_data->type) {
        case FB_TYPE_RUMBLE:
            if (fb_data->state) {
                set_conf->hf_motor_pwr = fb_data->hf_pwr;
                set_conf->lf_motor_pwr = fb_data->lf_pwr;
            }
            else {
                set_conf->hf_motor_pwr = 0x00;
                set_conf->lf_motor_pwr = 0x00;
            }
            break;
        case FB_TYPE_PLAYER_LED:
            set_conf->leds = hw_config.ps_ctrl_colors[bt_data->base.pids->out_idx];
            break;
    }
}

int32_t ps_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    switch (bt_data->base.report_id) {
        case 0x01:
            hid_to_generic(bt_data, ctrl_data);
            break;
        case 0x11:
            ps4_to_generic(bt_data, ctrl_data);
            break;
        case 0x31:
            ps5_to_generic(bt_data, ctrl_data);
            break;
        default:
            printf("# Unknown report type: %02lX\n", bt_data->base.report_type);
            return -1;
    }

    return 0;
}

bool ps_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data) {
    bool ret = true;
    switch (bt_data->base.pids->subtype) {
        case BT_PS5_DS:
            ps5_fb_from_generic(fb_data, bt_data);
            break;
        default:
            ps4_fb_from_generic(fb_data, bt_data);
            break;
    }
    return ret;
}
