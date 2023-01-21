/*
 * Copyright (c) 2021-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter.h"
#include "mapping_quirks.h"

static void face_btns_invert(struct raw_src_mapping *map) {
    uint32_t tmp = map->btns_mask[PAD_RB_LEFT];

    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_UP];
    map->btns_mask[PAD_RB_UP] = tmp;

    tmp = map->btns_mask[PAD_RB_DOWN];
    map->btns_mask[PAD_RB_DOWN] = map->btns_mask[PAD_RB_RIGHT];
    map->btns_mask[PAD_RB_RIGHT] = tmp;
}

static void face_btns_rotate_right(struct raw_src_mapping *map) {
    uint32_t tmp = map->btns_mask[PAD_RB_UP];

    map->btns_mask[PAD_RB_UP] = map->btns_mask[PAD_RB_LEFT];
    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_DOWN];
    map->btns_mask[PAD_RB_DOWN] = map->btns_mask[PAD_RB_RIGHT];
    map->btns_mask[PAD_RB_RIGHT] = tmp;
}

static void face_btns_trigger_to_6buttons(struct raw_src_mapping *map) {
    map->desc[0] = 0x000000FF;

    map->btns_mask[PAD_LM] = map->btns_mask[PAD_RB_LEFT];
    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_DOWN];
    map->btns_mask[PAD_RB_DOWN] = map->btns_mask[PAD_RB_RIGHT];
    map->btns_mask[PAD_RB_RIGHT] = 0;

    map->axes_to_btns[TRIG_L] = PAD_RS;
    map->axes_to_btns[TRIG_R] = PAD_RB_RIGHT;
}

static void trigger_pri_sec_invert(struct raw_src_mapping *map) {
    uint32_t tmp = map->btns_mask[PAD_LM];

    map->btns_mask[PAD_LM] = map->btns_mask[PAD_LS];
    map->btns_mask[PAD_LS] = tmp;

    tmp = map->btns_mask[PAD_RM];
    map->btns_mask[PAD_RM] = map->btns_mask[PAD_RS];
    map->btns_mask[PAD_RS] = tmp;
}

static void sw_left_joycon(struct raw_src_mapping *map) {
    map->btns_mask[PAD_MM] = map->btns_mask[PAD_MQ];
    map->btns_mask[PAD_MQ] = 0;

    map->btns_mask[PAD_RM] = map->btns_mask[PAD_RT];
    map->btns_mask[PAD_RS] = map->btns_mask[PAD_LS];
    map->btns_mask[PAD_RT] = 0;

    map->btns_mask[PAD_LS] = map->btns_mask[PAD_LM];
    map->btns_mask[PAD_LM] = map->btns_mask[PAD_LT];
    map->btns_mask[PAD_LT] = 0;

    map->btns_mask[PAD_RB_UP] = map->btns_mask[PAD_LD_RIGHT];
    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_LD_UP];
    map->btns_mask[PAD_RB_DOWN] = map->btns_mask[PAD_LD_LEFT];
    map->btns_mask[PAD_RB_RIGHT] = map->btns_mask[PAD_LD_DOWN];

    map->btns_mask[PAD_LD_LEFT] = 0;
    map->btns_mask[PAD_LD_DOWN] = 0;
    map->btns_mask[PAD_LD_RIGHT] = 0;
    map->btns_mask[PAD_LD_UP] = 0;
}

static void sw_right_joycon(struct raw_src_mapping *map) {
    map->btns_mask[PAD_MS] = map->btns_mask[PAD_MT];
    map->btns_mask[PAD_MT] = 0;

    map->btns_mask[PAD_LM] = map->btns_mask[PAD_LT];
    map->btns_mask[PAD_LS] = map->btns_mask[PAD_RS];
    map->btns_mask[PAD_LT] = 0;
    map->btns_mask[PAD_LJ] = map->btns_mask[PAD_RJ];

    map->btns_mask[PAD_RS] = map->btns_mask[PAD_RM];
    map->btns_mask[PAD_RM] = map->btns_mask[PAD_RT];
    map->btns_mask[PAD_RT] = 0;
    map->btns_mask[PAD_RJ] = 0;

    face_btns_rotate_right(map);
}

static void n64_8bitdo(struct raw_src_mapping *map) {
    map->mask[0] = 0x23150FFF;
    map->desc[0] = 0x0000000F;

    map->btns_mask[PAD_RY_UP] = BIT(8);
    map->btns_mask[PAD_RX_RIGHT] = BIT(9);
    map->btns_mask[PAD_RY_DOWN] = map->btns_mask[PAD_RB_LEFT];
    map->btns_mask[PAD_RX_LEFT] = map->btns_mask[PAD_RB_UP];
    map->btns_mask[PAD_LM] = map->btns_mask[PAD_MS];
    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_RIGHT];
    map->btns_mask[PAD_RB_RIGHT] = 0;
    map->btns_mask[PAD_RB_UP] = 0;
    map->btns_mask[PAD_MS] = 0;
    map->btns_mask[PAD_RT] = 0;
}

static void m30_8bitdo(struct raw_src_mapping *map) {
    map->desc[0] = 0x000000FF;

    map->btns_mask[PAD_LM] = map->btns_mask[PAD_LS];
    map->btns_mask[PAD_LS] = map->btns_mask[PAD_RB_LEFT];
    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_DOWN];
    map->btns_mask[PAD_RB_DOWN] = map->btns_mask[PAD_RB_RIGHT];
    map->btns_mask[PAD_RB_RIGHT] = 0;

    map->axes_to_btns[TRIG_L] = PAD_RM;
    map->axes_to_btns[TRIG_R] = PAD_RB_RIGHT;
}

static void saturn_diy_8bitdo(struct raw_src_mapping *map) {
    uint32_t tmp = map->btns_mask[PAD_LS];

    map->btns_mask[PAD_LS] = map->btns_mask[PAD_RB_LEFT];
    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_DOWN];
    map->btns_mask[PAD_RB_DOWN] = map->btns_mask[PAD_RB_RIGHT];
    map->btns_mask[PAD_RB_RIGHT] = map->btns_mask[PAD_RS];
    map->btns_mask[PAD_RS] = tmp;
}

static void n64_bluen64(struct raw_src_mapping *map) {
    map->mask[0] = 0x23150FFF;
    map->desc[0] = 0x000000FF;

    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_RIGHT];
    map->btns_mask[PAD_RX_LEFT] = BIT(3);
    map->btns_mask[PAD_RY_DOWN] = BIT(4);
    map->btns_mask[PAD_LM] = BIT(8);
    map->btns_mask[PAD_MM] = BIT(5);
}

static void rf_warrior(struct raw_src_mapping *map) {
    map->mask[0] = 0x337F0FFF;
    map->desc[0] = 0x000000FF;

    map->btns_mask[PAD_RB_LEFT] = BIT(1);
    map->btns_mask[PAD_RB_RIGHT] = BIT(2);
    map->btns_mask[PAD_LM] = BIT(4);
    map->btns_mask[PAD_LS] = 0;
    map->btns_mask[PAD_RM] = BIT(5);
    map->btns_mask[PAD_RS] = BIT(6);
    map->btns_mask[PAD_MS] = 0;
}

enum {
    STADIA_CAPTURE = 0,
    STADIA_ASSISTANT,
    STADIA_LT_L2,
    STADIA_RT_R2,
    STADIA_STADIA,
    STADIA_MENU,
    STADIA_OPTIONS,
    STADIA_R3,
    STADIA_L3,
    STADIA_RB_R1,
    STADIA_LB_L1,
    STADIA_Y,
    STADIA_X,
    STADIA_B,
    STADIA_A,
};

static void stadia(struct raw_src_mapping *map) {
    map->btns_mask[PAD_RD_LEFT] = BIT(STADIA_CAPTURE);
    map->btns_mask[PAD_RB_LEFT] = BIT(STADIA_X);
    map->btns_mask[PAD_RB_RIGHT] = BIT(STADIA_B);
    map->btns_mask[PAD_RB_DOWN] = BIT(STADIA_A);
    map->btns_mask[PAD_RB_UP] = BIT(STADIA_Y);
    map->btns_mask[PAD_MM] = BIT(STADIA_MENU);
    map->btns_mask[PAD_MS] = BIT(STADIA_OPTIONS);
    map->btns_mask[PAD_MT] = BIT(STADIA_STADIA);
    map->btns_mask[PAD_MQ] = BIT(STADIA_ASSISTANT);
    map->btns_mask[PAD_LM] = 0;
    map->btns_mask[PAD_LS] = BIT(STADIA_LB_L1);
    map->btns_mask[PAD_LJ] = BIT(STADIA_L3);
    map->btns_mask[PAD_RM] = 0;
    map->btns_mask[PAD_RS] = BIT(STADIA_RB_R1);
    map->btns_mask[PAD_RJ] = BIT(STADIA_R3);
}

void mapping_quirks_apply(struct bt_data *bt_data) {
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_FACE_BTNS_INVERT)) {
        face_btns_invert(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_FACE_BTNS_ROTATE_RIGHT)) {
        face_btns_rotate_right(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_FACE_BTNS_TRIGGER_TO_6BUTTONS)) {
        face_btns_trigger_to_6buttons(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_TRIGGER_PRI_SEC_INVERT)) {
        trigger_pri_sec_invert(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_SW_LEFT_JOYCON)) {
        sw_left_joycon(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_SW_RIGHT_JOYCON)) {
        sw_right_joycon(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_8BITDO_N64)) {
        n64_8bitdo(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_8BITDO_M30)) {
        /* Only load this quirk if 8bitdo is connected in xinput mode */
        if (bt_data->base.report_id == 0x01) {
            m30_8bitdo(&bt_data->raw_src_mappings[PAD]);
        }
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_8BITDO_SATURN)) {
        saturn_diy_8bitdo(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_BLUEN64_N64)) {
        n64_bluen64(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_RF_WARRIOR)) {
        rf_warrior(&bt_data->raw_src_mappings[PAD]);
    }
        if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_STADIA)) {
        stadia(&bt_data->raw_src_mappings[PAD]);
    }
}
