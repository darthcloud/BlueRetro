/*
 * Copyright (c) 2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "adapter/mapping_quirks.h"
#include "tests/cmds.h"
#include "bluetooth/mon.h"
#include "bluetooth/hidp/sw2.h"
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

static const uint8_t sw2_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       2,       3,       4,      5
};

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

static int32_t sw2_pad_init(struct bt_data *bt_data) {
    struct bt_hid_sw2_ctrl_calib *calib = NULL;
    const uint8_t *axes_idx = sw2_axes_idx;
    struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;
    const struct ctrl_meta *sw2_axes_meta = sw2_pro_axes_meta;

    mapping_quirks_apply(bt_data);

    bt_hid_sw2_get_calib(bt_data->base.pids->id, &calib);

    switch (bt_data->base.pid) {
        case SW2_LJC_PID:
        {
            // memcpy(bt_data->raw_src_mappings[PAD].btns_mask, &sw_jc_btns_mask[report_type],
            //     sizeof(bt_data->raw_src_mappings[PAD].btns_mask));

            // meta[0].polarity = 1;
            // meta[1].polarity = 0;
            // axes_idx = sw_jc_axes_idx;
            // memcpy(bt_data->raw_src_mappings[PAD].mask, sw_jc_mask,
            //     sizeof(bt_data->raw_src_mappings[PAD].mask));
            // memcpy(bt_data->raw_src_mappings[PAD].desc, desc,
            //     sizeof(bt_data->raw_src_mappings[PAD].desc));
            break;
        }
        case SW2_RJC_PID:
        {
            // memcpy(bt_data->raw_src_mappings[PAD].btns_mask, &sw_jc_btns_mask[report_type],
            //     sizeof(bt_data->raw_src_mappings[PAD].btns_mask));

            // meta[0].polarity = 0;
            // meta[1].polarity = 1;
            // axes_idx = sw_jc_axes_idx;
            // memcpy(bt_data->raw_src_mappings[PAD].mask, sw_jc_mask,
            //     sizeof(bt_data->raw_src_mappings[PAD].mask));
            // memcpy(bt_data->raw_src_mappings[PAD].desc, desc,
            //     sizeof(bt_data->raw_src_mappings[PAD].desc));
            break;
        }
        case SW2_GC_PID:
        {
            memcpy(bt_data->raw_src_mappings[PAD].btns_mask, sw2_gc_btns_mask,
                sizeof(bt_data->raw_src_mappings[PAD].btns_mask));
            meta[0].polarity = 0;
            meta[1].polarity = 0;
            meta[2].polarity = 0;
            meta[3].polarity = 0;
            axes_idx = sw2_axes_idx;
            sw2_axes_meta = sw2_gc_axes_meta;
            memcpy(bt_data->raw_src_mappings[PAD].mask, sw2_gc_mask,
                sizeof(bt_data->raw_src_mappings[PAD].mask));
            memcpy(bt_data->raw_src_mappings[PAD].desc, sw2_gc_desc,
                sizeof(bt_data->raw_src_mappings[PAD].desc));

            meta[TRIG_L].neutral = sw2_gc_axes_meta[TRIG_L].neutral;
            meta[TRIG_L].abs_max = sw2_gc_axes_meta[TRIG_L].abs_max;
            meta[TRIG_L].abs_min = sw2_gc_axes_meta[TRIG_L].abs_min;
            meta[TRIG_R].neutral = sw2_gc_axes_meta[TRIG_R].neutral;
            meta[TRIG_R].abs_max = sw2_gc_axes_meta[TRIG_R].abs_max;
            meta[TRIG_R].abs_min = sw2_gc_axes_meta[TRIG_R].abs_min;
            break;
        }
        case SW2_PRO2_PID:
        default:
        {
            memcpy(bt_data->raw_src_mappings[PAD].btns_mask, sw2_pro_btns_mask,
                sizeof(bt_data->raw_src_mappings[PAD].btns_mask));
            meta[0].polarity = 0;
            meta[1].polarity = 0;
            meta[2].polarity = 0;
            meta[3].polarity = 0;
            axes_idx = sw2_axes_idx;
            memcpy(bt_data->raw_src_mappings[PAD].mask, sw2_pro_mask,
                sizeof(bt_data->raw_src_mappings[PAD].mask));
            memcpy(bt_data->raw_src_mappings[PAD].desc, sw2_pro_desc,
                sizeof(bt_data->raw_src_mappings[PAD].desc));
            break;
        }
    }

    for (uint32_t i = 0; i < SW2_AXES_MAX; i++) {
        if (calib && calib->sticks[i / 2].axes[i % 2].neutral) {
            meta[axes_idx[i]].neutral = calib->sticks[i / 2].axes[i % 2].neutral;
            meta[axes_idx[i]].abs_max = calib->sticks[i / 2].axes[i % 2].rel_max * MAX_PULL_BACK;
            meta[axes_idx[i]].abs_min = calib->sticks[i / 2].axes[i % 2].rel_min * MAX_PULL_BACK;
            meta[axes_idx[i]].deadzone = calib->sticks[i / 2].deadzone;
            printf("# %s: controller calib loaded\n", __FUNCTION__);
        }
        else {
            meta[axes_idx[i]].neutral = sw2_axes_meta[i].neutral;
            meta[axes_idx[i]].abs_max = sw2_axes_meta[i].abs_max * MAX_PULL_BACK;
            meta[axes_idx[i]].abs_min = sw2_axes_meta[i].abs_min * MAX_PULL_BACK;
            meta[axes_idx[i]].deadzone = sw2_axes_meta[i].deadzone;
            printf("# %s: no calib, using default\n", __FUNCTION__);
        }
        bt_data->base.axes_cal[i] = 0;
    }

    atomic_set_bit(&bt_data->base.flags[PAD], BT_INIT);
    return 0;
}

static int32_t sw2_pro_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    struct sw2_map *map = (struct sw2_map *)bt_data->base.input;
    struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;
    uint16_t axes[4];

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        if (sw2_pad_init(bt_data)) {
            return -1;
        }
    }

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

    TESTS_CMDS_LOG("\"wireless_input\": {\"axes\": [%u, %u, %u, %u], \"btns\": %lu},\n",
        axes[0], axes[1], axes[2], axes[3], map->buttons);

    for (uint32_t i = 0; i < SW2_AXES_MAX; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = axes[i] - meta[i].neutral;
    }
    return 0;
}

static int32_t sw2_gc_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    
    struct sw2_map *map = (struct sw2_map *)bt_data->base.input;
    struct ctrl_meta *meta = bt_data->raw_src_mappings[PAD].meta;
    uint16_t axes[4];

    if (!atomic_test_bit(&bt_data->base.flags[PAD], BT_INIT)) {
        if (sw2_pad_init(bt_data)) {
            return -1;
        }
    }

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

    TESTS_CMDS_LOG("\"wireless_input\": {\"axes\": [%u, %u, %u, %u], \"btns\": %lu},\n",
        axes[0], axes[1], axes[2], axes[3], map->buttons);

    for (uint32_t i = 0; i < SW2_AXES_MAX; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = axes[i] - meta[i].neutral;
    }
    for (uint32_t i = 4; i < ADAPTER_MAX_AXES; i++) {
        ctrl_data->axes[i].meta = &meta[i];
        ctrl_data->axes[i].value = map->triggers[i - 4] - meta[i].neutral;
    }
    return 0;
}

int32_t sw2_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    switch (bt_data->base.pid) {
        case SW2_LJC_PID:
        case SW2_RJC_PID:
        case SW2_PRO2_PID:
            return sw2_pro_to_generic(bt_data, ctrl_data);
        case SW2_GC_PID:
            return sw2_gc_to_generic(bt_data, ctrl_data);
        default:
            printf("# Unknown pid : %04X\n", bt_data->base.pid);
            return -1;
    }
    return 0;
}

bool sw2_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data) {
    struct bt_hidp_sw2_out *out = (struct bt_hidp_sw2_out *)bt_data->base.output;
    bool ret = true;

    switch (bt_data->base.pid) {
        case SW2_LJC_PID:
        case SW2_RJC_PID:
        case SW2_PRO2_PID:
            switch (fb_data->type) {
                case FB_TYPE_RUMBLE:
                    if (fb_data->hf_pwr || fb_data->lf_pwr) {
                        out->r_lra.ops[0].hf_freq = BT_HIDP_SW2_LRA_R_HF_FREQ;
                        out->r_lra.ops[0].hf_amp = (uint8_t)((float)fb_data->hf_pwr / 2.68);
                        out->r_lra.ops[0].lf_freq = BT_HIDP_SW2_LRA_R_LF_FREQ;
                        out->r_lra.ops[0].lf_amp = (uint16_t)((float)fb_data->hf_pwr / 0.3156);
                        out->r_lra.ops[0].enable = 1;

                        out->l_lra.ops[0].hf_freq = BT_HIDP_SW2_LRA_L_HF_FREQ;
                        out->l_lra.ops[0].hf_amp = (uint8_t)((float)fb_data->lf_pwr / 2.68);
                        out->l_lra.ops[0].lf_freq = BT_HIDP_SW2_LRA_L_LF_FREQ;
                        out->l_lra.ops[0].lf_amp = (uint16_t)((float)fb_data->lf_pwr / 0.3156);
                        out->l_lra.ops[0].enable = 1;
                    }
                    else {
                        out->l_lra.ops[0].val = BT_HIDP_SW2_LRA_IDLE_32;
                        out->l_lra.ops[0].hf_amp = BT_HIDP_SW2_LRA_IDLE_8;
                        out->r_lra.ops[0].val = BT_HIDP_SW2_LRA_IDLE_32;
                        out->r_lra.ops[0].hf_amp = BT_HIDP_SW2_LRA_IDLE_8;
                    }
                    //printf("%08lX %02X %08lX %02X\n", out->l_lra.ops[0].val, out->l_lra.ops[0].hf_amp, out->r_lra.ops[0].val, out->r_lra.ops[0].hf_amp);
                    break;
                case FB_TYPE_PLAYER_LED:
                    bt_data->base.output[41] = bt_hid_led_dev_id_map[bt_data->base.pids->out_idx];
                    break;
            }
            break;
        case SW2_GC_PID:
        {
            uint8_t cur_val = bt_data->base.output[2];
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
            ret = (cur_val != bt_data->base.output[2]);
            break;
        }
        default:
            printf("# Unknown pid : %04X\n", bt_data->base.pid);
    }
    return ret;
}
