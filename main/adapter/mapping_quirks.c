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

void mapping_quirks_apply(struct bt_data *bt_data) {
    if (atomic_test_bit(&bt_data->flags, BT_QUIRK_FACE_BTNS_INVERT)) {
        face_btns_invert(bt_data->raw_src_mappings[PAD].btns_mask);
    }
    if (atomic_test_bit(&bt_data->flags, BT_QUIRK_FACE_BTNS_ROTATE_RIGHT)) {
        face_btns_rotate_right(bt_data->raw_src_mappings[PAD].btns_mask);
    }
}
