/*
 * Copyright (c) 2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "tests/cmds.h"
#include "bluetooth/mon.h"
#include "bluetooth/hidp/hidp.h"
#include "sw2.h"

#define SW2_AXES_MAX 4

enum {
    SW2_Y = 0,
    SW2_X,
    SW2_B,
    SW2_A,
    SW2_R_SR,
    SW2_R_SL,
    SW2_R,
    SW2_ZR,
    SW2_MINUS,
    SW2_PLUS,
    SW2_RJ,
    SW2_LJ,
    SW2_HOME,
    SW2_CAPTURE,
    SW2_C,
    SW2_UNKNOWN,
    SW2_DOWN,
    SW2_UP,
    SW2_RIGHT,
    SW2_LEFT,
    SW2_L_SR,
    SW2_L_SL,
    SW2_L,
    SW2_ZL,
    SW2_GR,
    SW2_GL,
};

// static const uint8_t sw2_axes_idx[ADAPTER_MAX_AXES] =
// {
// /*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
//     0,       1,       2,       3,       4,      5
// };

static const struct ctrl_meta sw2_pro_axes_meta[ADAPTER_MAX_AXES] =
{
    {.neutral = 0x800, .abs_max = 1610, .abs_min = 1610},
    {.neutral = 0x800, .abs_max = 1610, .abs_min = 1610},
    {.neutral = 0x800, .abs_max = 1610, .abs_min = 1610},
    {.neutral = 0x800, .abs_max = 1610, .abs_min = 1610},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
};

static const struct ctrl_meta sw2_gc_axes_meta[ADAPTER_MAX_AXES] =
{
    {.neutral = 0x800, .abs_max = 1225, .abs_min = 1225},
    {.neutral = 0x800, .abs_max = 1225, .abs_min = 1225},
    {.neutral = 0x800, .abs_max = 1120, .abs_min = 1120},
    {.neutral = 0x800, .abs_max = 1120, .abs_min = 1120},
    {.neutral = 30, .abs_max = 195, .abs_min = 0x00},
    {.neutral = 30, .abs_max = 195, .abs_min = 0x00},
};

struct sw2_map {
    uint8_t tbd[4];
    uint32_t buttons;
    uint8_t tbd1[2];
    uint8_t axes[6];
    uint8_t tbd2[44];
    uint8_t triggers[2];
    uint8_t tbd3;
} __packed;

static const uint32_t sw2_pro_mask[4] = {0xFFFF1FFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw2_pro_desc[4] = {0x000000FF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw2_pro_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(SW2_LEFT), BIT(SW2_RIGHT), BIT(SW2_DOWN), BIT(SW2_UP),
    BIT(SW2_C), 0, 0, 0,
    BIT(SW2_Y), BIT(SW2_A), BIT(SW2_B), BIT(SW2_X),
    BIT(SW2_PLUS), BIT(SW2_MINUS), BIT(SW2_HOME), BIT(SW2_CAPTURE),
    BIT(SW2_ZL), BIT(SW2_L), BIT(SW2_GL), BIT(SW2_LJ),
    BIT(SW2_ZR), BIT(SW2_R), BIT(SW2_GR), BIT(SW2_RJ),
};

static void sw2_pro_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    struct sw2_map *map = (struct sw2_map *)bt_data->base.input;
    struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;
    uint16_t axes[4];

    // TESTS_CMDS_LOG("\"wireless_input\": {\"report_id\": %ld, \"axes\": [%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u]], \"btns\": %lu},\n",
    //     bt_data->base.report_id,
    //     map->axes[0], map->axes[1], map->axes[2], map->axes[3], map->axes[4], map->axes[5],
    //     map->axes[6], map->axes[7], map->axes[8], map->axes[9], map->axes[10], map->axes[11],
    //     map->buttons);

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)sw2_pro_mask;
    ctrl_data->desc = (uint32_t *)sw2_pro_desc;

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (map->buttons & sw2_pro_btns_mask[i]) {
            ctrl_data->btns[0].value |= generic_btns_mask[i];
        }
    }

    axes[0] = map->axes[0] | ((map->axes[1] & 0xF) << 8);
    axes[1] = (map->axes[1] >> 4) | (map->axes[2] << 4);
    axes[2] = map->axes[3] | ((map->axes[4] & 0xF) << 8);
    axes[3] = (map->axes[4] >> 4) | (map->axes[5] << 4);

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        memcpy(meta, sw2_pro_axes_meta, sizeof(sw2_pro_axes_meta));
        // bt_mon_log(false, "%s: axes_cal: [", __FUNCTION__);
        // for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        //     meta[i].abs_max *= MAX_PULL_BACK;
        //     meta[i].abs_min *= MAX_PULL_BACK;
        //     bt_data->base.axes_cal[i] = -(map->axes[sw2_pro_axes_idx[i]] - sw2_pro_axes_meta[i].neutral);
        //     if (i) {
        //         bt_mon_log(false, ", ");
        //     }
        //     bt_mon_log(false, "%d", bt_data->base.axes_cal[i]);
        // }
        atomic_set_bit(&bt_data->base.flags[PAD], BT_INIT);
        // bt_mon_log(true, "]");
    }

    for (uint32_t i = 0; i < SW2_AXES_MAX; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = axes[i] - meta[i].neutral;
    }
}

static const uint32_t sw2_gc_mask[4] = {0x77FF0FFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw2_gc_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sw2_gc_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(SW2_LEFT), BIT(SW2_RIGHT), BIT(SW2_DOWN), BIT(SW2_UP),
    0, 0, 0, 0,
    BIT(SW2_B), BIT(SW2_X), BIT(SW2_A), BIT(SW2_Y),
    BIT(SW2_PLUS), BIT(SW2_C), BIT(SW2_HOME), BIT(SW2_CAPTURE),
    0, BIT(SW2_ZL), BIT(SW2_L), 0,
    0, BIT(SW2_ZR), BIT(SW2_R), 0,
};

static void sw2_gc_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    
    struct sw2_map *map = (struct sw2_map *)bt_data->base.input;
    struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;
    uint16_t axes[4];

    // TESTS_CMDS_LOG("\"wireless_input\": {\"report_id\": %ld, \"axes\": [%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u]], \"btns\": %lu},\n",
    //     bt_data->base.report_id,
    //     map->axes[0], map->axes[1], map->axes[2], map->axes[3], map->axes[4], map->axes[5],
    //     map->axes[6], map->axes[7], map->axes[8], map->axes[9], map->axes[10], map->axes[11],
    //     map->buttons);

    memset((void *)ctrl_data, 0, sizeof(*ctrl_data));

    ctrl_data->mask = (uint32_t *)sw2_gc_mask;
    ctrl_data->desc = (uint32_t *)sw2_gc_desc;

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (map->buttons & sw2_gc_btns_mask[i]) {
            ctrl_data->btns[0].value |= generic_btns_mask[i];
        }
    }

    axes[0] = map->axes[0] | ((map->axes[1] & 0xF) << 8);
    axes[1] = (map->axes[1] >> 4) | (map->axes[2] << 4);
    axes[2] = map->axes[3] | ((map->axes[4] & 0xF) << 8);
    axes[3] = (map->axes[4] >> 4) | (map->axes[5] << 4);

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        memcpy(meta, sw2_gc_axes_meta, sizeof(sw2_gc_axes_meta));
        // bt_mon_log(false, "%s: axes_cal: [", __FUNCTION__);
        // for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        //     meta[i].abs_max *= MAX_PULL_BACK;
        //     meta[i].abs_min *= MAX_PULL_BACK;
        //     bt_data->base.axes_cal[i] = -(map->axes[sw2_pro_axes_idx[i]] - sw2_pro_axes_meta[i].neutral);
        //     if (i) {
        //         bt_mon_log(false, ", ");
        //     }
        //     bt_mon_log(false, "%d", bt_data->base.axes_cal[i]);
        // }
        atomic_set_bit(&bt_data->base.flags[PAD], BT_INIT);
        // bt_mon_log(true, "]");
    }

    for (uint32_t i = 0; i < SW2_AXES_MAX; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = axes[i] - meta[i].neutral;
    }
    for (uint32_t i = 4; i < ADAPTER_MAX_AXES; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = map->triggers[i - 4] - meta[i].neutral;
    }
}

int32_t sw2_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    switch (bt_data->base.pid) {
        case 0x2066:
        case 0x2067:
        case 0x2069:
            sw2_pro_to_generic(bt_data, ctrl_data);
            break;
        case 0x2073:
            sw2_gc_to_generic(bt_data, ctrl_data);
            break;
        default:
            printf("# Unknown pid : %04X\n", bt_data->base.pid);
            return -1;
    }
    return 0;
}

void sw2_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data) {
    switch (bt_data->base.pid) {
        case 0x2066:
        case 0x2067:
        case 0x2069:
            break;
        case 0x2073:
            switch (fb_data->type) {
                case FB_TYPE_RUMBLE:
                    if (fb_data->state) {
                        bt_data->base.output[2] = 0x01;
                    }
                    else {
                        bt_data->base.output[2] = 0x00;
                    }
                    break;
                case FB_TYPE_PLAYER_LED:
                    bt_data->base.output[13] = bt_hid_led_dev_id_map[bt_data->base.pids->out_idx];
                    break;
            }
            break;
        default:
            printf("# Unknown pid : %04X\n", bt_data->base.pid);
    }
}
