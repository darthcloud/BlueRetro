/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter.h"
#include "mapping_quirks.h"

static void face_btns_invert(uint32_t btns_mask[32]) {
    uint32_t tmp = btns_mask[PAD_RB_LEFT];

    btns_mask[PAD_RB_LEFT] = btns_mask[PAD_RB_UP];
    btns_mask[PAD_RB_UP] = tmp;

    tmp = btns_mask[PAD_RB_DOWN];
    btns_mask[PAD_RB_DOWN] = btns_mask[PAD_RB_RIGHT];
    btns_mask[PAD_RB_RIGHT] = tmp;
}

static void face_btns_rotate_right(uint32_t btns_mask[32]) {
    uint32_t tmp = btns_mask[PAD_RB_UP];

    btns_mask[PAD_RB_UP] = btns_mask[PAD_RB_LEFT];
    btns_mask[PAD_RB_LEFT] = btns_mask[PAD_RB_DOWN];
    btns_mask[PAD_RB_DOWN] = btns_mask[PAD_RB_RIGHT];
    btns_mask[PAD_RB_RIGHT] = tmp;
}

static void trigger_pri_sec_invert(uint32_t btns_mask[32]) {
    uint32_t tmp = btns_mask[PAD_LM];

    btns_mask[PAD_LM] = btns_mask[PAD_LS];
    btns_mask[PAD_LS] = tmp;

    tmp = btns_mask[PAD_RM];
    btns_mask[PAD_RM] = btns_mask[PAD_RS];
    btns_mask[PAD_RS] = tmp;
}

static void sw_left_joycon(uint32_t btns_mask[32]) {
    btns_mask[PAD_MM] = btns_mask[PAD_MQ];
    btns_mask[PAD_MQ] = 0;

    btns_mask[PAD_RM] = btns_mask[PAD_RT];
    btns_mask[PAD_RS] = btns_mask[PAD_LS];
    btns_mask[PAD_RT] = 0;

    btns_mask[PAD_LS] = btns_mask[PAD_LM];
    btns_mask[PAD_LM] = btns_mask[PAD_LT];
    btns_mask[PAD_LT] = 0;

    btns_mask[PAD_RB_UP] = btns_mask[PAD_LD_RIGHT];
    btns_mask[PAD_RB_LEFT] = btns_mask[PAD_LD_UP];
    btns_mask[PAD_RB_DOWN] = btns_mask[PAD_LD_LEFT];
    btns_mask[PAD_RB_RIGHT] = btns_mask[PAD_LD_DOWN];

    btns_mask[PAD_LD_LEFT] = 0;
    btns_mask[PAD_LD_DOWN] = 0;
    btns_mask[PAD_LD_RIGHT] = 0;
    btns_mask[PAD_LD_UP] = 0;
}

static void sw_right_joycon(uint32_t btns_mask[32]) {
    btns_mask[PAD_MS] = btns_mask[PAD_MT];
    btns_mask[PAD_MT] = 0;

    btns_mask[PAD_LM] = btns_mask[PAD_LT];
    btns_mask[PAD_LS] = btns_mask[PAD_RS];
    btns_mask[PAD_LT] = 0;
    btns_mask[PAD_LJ] = btns_mask[PAD_RJ];

    btns_mask[PAD_RS] = btns_mask[PAD_RM];
    btns_mask[PAD_RM] = btns_mask[PAD_RT];
    btns_mask[PAD_RT] = 0;
    btns_mask[PAD_RJ] = 0;

    face_btns_rotate_right(btns_mask);
}

void mapping_quirks_apply(struct bt_data *bt_data) {
    if (atomic_test_bit(&bt_data->flags, BT_QUIRK_FACE_BTNS_INVERT)) {
        face_btns_invert(bt_data->raw_src_mappings[PAD].btns_mask);
    }
    if (atomic_test_bit(&bt_data->flags, BT_QUIRK_FACE_BTNS_ROTATE_RIGHT)) {
        face_btns_rotate_right(bt_data->raw_src_mappings[PAD].btns_mask);
    }
    if (atomic_test_bit(&bt_data->flags, BT_QUIRK_TRIGGER_PRI_SEC_INVERT)) {
        trigger_pri_sec_invert(bt_data->raw_src_mappings[PAD].btns_mask);
    }
    if (atomic_test_bit(&bt_data->flags, BT_QUIRK_SW_LEFT_JOYCON)) {
        sw_left_joycon(bt_data->raw_src_mappings[PAD].btns_mask);
    }
    if (atomic_test_bit(&bt_data->flags, BT_QUIRK_SW_RIGHT_JOYCON)) {
        sw_right_joycon(bt_data->raw_src_mappings[PAD].btns_mask);
    }
}
