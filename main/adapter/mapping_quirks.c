/*
 * Copyright (c) 2021-2025, Jacques Gagnon
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
    map->desc[0] = 0x200200FF;

    map->btns_mask[PAD_LM] = map->btns_mask[PAD_RB_LEFT];
    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_DOWN];
    map->btns_mask[PAD_RB_DOWN] = map->btns_mask[PAD_RB_RIGHT];
    map->btns_mask[PAD_RB_RIGHT] = 0;

    map->axes_idx[TRIG_RS] = map->axes_idx[TRIG_L];
    map->axes_idx[TRIG_L] = -1;
    memcpy(&map->meta[TRIG_RS], &map->meta[TRIG_L], sizeof(map->meta[0]));

    map->axes_idx[BTN_RIGHT] = map->axes_idx[TRIG_R];
    map->axes_idx[TRIG_R] = -1;
    memcpy(&map->meta[BTN_RIGHT], &map->meta[TRIG_R], sizeof(map->meta[0]));
}

static void face_btns_trigger_to_8buttons(struct raw_src_mapping *map) {
    map->desc[0] = 0x100200FF;

    map->btns_mask[PAD_LM] = map->btns_mask[PAD_LS];
    map->btns_mask[PAD_LS] = map->btns_mask[PAD_RB_LEFT];
    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_DOWN];
    map->btns_mask[PAD_RB_DOWN] = map->btns_mask[PAD_RB_RIGHT];

    map->btns_mask[PAD_RB_RIGHT] = 0;
    map->axes_idx[BTN_RIGHT] = map->axes_idx[TRIG_R];
    memcpy(&map->meta[BTN_RIGHT], &map->meta[TRIG_R], sizeof(map->meta[0]));

    map->axes_idx[TRIG_R] = map->axes_idx[TRIG_L];
    map->axes_idx[TRIG_L] = -1;
    memcpy(&map->meta[TRIG_R], &map->meta[TRIG_L], sizeof(map->meta[0]));
}

static void trigger_pri_sec_invert(struct raw_src_mapping *map) {
    uint32_t tmp = map->btns_mask[PAD_LM];

    map->desc[0] &= ~0x11000000;

    map->btns_mask[PAD_LM] = map->btns_mask[PAD_LS];
    map->btns_mask[PAD_LS] = tmp;

    tmp = map->btns_mask[PAD_RM];
    map->btns_mask[PAD_RM] = map->btns_mask[PAD_RS];
    map->btns_mask[PAD_RS] = tmp;
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

static void n64_8bitdo_mk(struct raw_src_mapping *map) {
    map->mask[0] = 0x33550FFF;
    map->desc[0] = 0x000000FF;

    map->btns_mask[PAD_RB_LEFT] = BIT(1);
    map->btns_mask[PAD_RB_RIGHT] = 0;
    map->btns_mask[PAD_MT] = BIT(12);
    map->btns_mask[PAD_LM] = BIT(8);
    map->btns_mask[PAD_RM] = BIT(9);
}

static void m30_8bitdo(struct raw_src_mapping *map) {
    map->desc[0] = 0x100200FF;

    map->btns_mask[PAD_LM] = map->btns_mask[PAD_LS];
    map->btns_mask[PAD_LS] = map->btns_mask[PAD_RB_LEFT];
    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_DOWN];
    map->btns_mask[PAD_RB_DOWN] = map->btns_mask[PAD_RB_RIGHT];
    map->btns_mask[PAD_RB_RIGHT] = 0;

    map->btns_mask[PAD_RB_RIGHT] = 0;
    map->axes_idx[BTN_RIGHT] = map->axes_idx[TRIG_R];
    memcpy(&map->meta[BTN_RIGHT], &map->meta[TRIG_R], sizeof(map->meta[0]));

    map->axes_idx[TRIG_R] = map->axes_idx[TRIG_L];
    map->axes_idx[TRIG_L] = -1;
    memcpy(&map->meta[TRIG_R], &map->meta[TRIG_L], sizeof(map->meta[0]));
}

static void m30_8bitdo_modkit(struct raw_src_mapping *map) {
    map->btns_mask[PAD_RB_LEFT] = BIT(0);
    map->btns_mask[PAD_RB_RIGHT] = BIT(5);
    map->btns_mask[PAD_RB_DOWN] = BIT(1);
    map->btns_mask[PAD_RB_UP] = BIT(3);
    map->btns_mask[PAD_LS] = BIT(2);
    map->btns_mask[PAD_RS] = BIT(4);
}

static void saturn_diy_8bitdo(struct raw_src_mapping *map) {
    uint32_t tmp = map->btns_mask[PAD_LS];

    map->btns_mask[PAD_LS] = map->btns_mask[PAD_RB_LEFT];
    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_DOWN];
    map->btns_mask[PAD_RB_DOWN] = map->btns_mask[PAD_RB_RIGHT];
    map->btns_mask[PAD_RB_RIGHT] = map->btns_mask[PAD_RS];
    map->btns_mask[PAD_RS] = tmp;
}

static void gc_diy_8bitdo(struct raw_src_mapping *map) {
    map->mask[0] = 0xFF1F0FFF;
    map->desc[0] = 0x110000FF;

    uint32_t tmp = map->btns_mask[PAD_RB_LEFT];
    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_RIGHT];
    map->btns_mask[PAD_RB_RIGHT] = tmp;
    map->btns_mask[PAD_RS] = map->btns_mask[PAD_MS];
    map->btns_mask[PAD_LT] = BIT(8);
    map->btns_mask[PAD_RT] = BIT(9);
    map->btns_mask[PAD_MS] = 0;
    map->btns_mask[PAD_LM] = 0;
    map->btns_mask[PAD_RM] = 0;
    map->btns_mask[PAD_LS] = 0;
}

static void gc_gbros_8bitdo(struct raw_src_mapping *map) {
    map->mask[0] = 0xFF5F0FFF;
    map->desc[0] = 0x110000FF;

    uint32_t tmp = map->btns_mask[PAD_RB_LEFT];
    map->btns_mask[PAD_RB_LEFT] = map->btns_mask[PAD_RB_RIGHT];
    map->btns_mask[PAD_RB_RIGHT] = tmp;
    map->btns_mask[PAD_LT] = map->btns_mask[PAD_LS];
    map->btns_mask[PAD_RT] = map->btns_mask[PAD_RS];
    map->btns_mask[PAD_RS] = map->btns_mask[PAD_MS];
    map->btns_mask[PAD_MS] = 0;
    map->btns_mask[PAD_LM] = 0;
    map->btns_mask[PAD_RM] = 0;
    map->btns_mask[PAD_LS] = 0;
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
    OUYA_O = 0,
    OUYA_U,
    OUYA_Y,
    OUYA_A,
    OUYA_L1,
    OUYA_R1,
    OUYA_L3,
    OUYA_R3,
    OUYA_UP,
    OUYA_DOWN,
    OUYA_LEFT,
    OUYA_RIGHT,
    OUYA_L2,
    OUYA_R2,
    OUYA_MENU,
};

static void ouya(struct raw_src_mapping *map) {
    map->mask[0] = 0xBB1F0FFF;
    map->desc[0] = 0x110000FF;

    memset(map->btns_mask, 0, sizeof(map->btns_mask));

    map->btns_mask[PAD_LD_LEFT] = BIT(OUYA_LEFT);
    map->btns_mask[PAD_LD_RIGHT] = BIT(OUYA_RIGHT);
    map->btns_mask[PAD_LD_DOWN] = BIT(OUYA_DOWN);
    map->btns_mask[PAD_LD_UP] = BIT(OUYA_UP);
    map->btns_mask[PAD_RB_LEFT] = BIT(OUYA_U);
    map->btns_mask[PAD_RB_RIGHT] = BIT(OUYA_A);
    map->btns_mask[PAD_RB_DOWN] = BIT(OUYA_O);
    map->btns_mask[PAD_RB_UP] = BIT(OUYA_Y);
    map->btns_mask[PAD_MM] = BIT(OUYA_MENU);
    map->btns_mask[PAD_LS] = BIT(OUYA_L1);
    map->btns_mask[PAD_LJ] = BIT(OUYA_L3);
    map->btns_mask[PAD_RS] = BIT(OUYA_R1);
    map->btns_mask[PAD_RJ] = BIT(OUYA_R3);
}

static void mapping_quirks_apply_pnp(struct bt_data *bt_data) {
    switch (bt_data->base.vid) {
    }
}

void mapping_quirks_apply(struct bt_data *bt_data) {
    mapping_quirks_apply_pnp(bt_data);
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_FACE_BTNS_INVERT)) {
        face_btns_invert(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_FACE_BTNS_ROTATE_RIGHT)) {
        face_btns_rotate_right(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_FACE_BTNS_TRIGGER_TO_6BUTTONS)) {
        face_btns_trigger_to_6buttons(&bt_data->raw_src_mappings[PAD]);
    }
    if (bt_data->base.vid == 0x045E && atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_FACE_BTNS_TRIGGER_TO_8BUTTONS)) {
        face_btns_trigger_to_8buttons(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_TRIGGER_PRI_SEC_INVERT)) {
        trigger_pri_sec_invert(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_8BITDO_N64)) {
        n64_8bitdo(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_8BITDO_N64_MK)) {
        n64_8bitdo_mk(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_8BITDO_M30)) {
        m30_8bitdo(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_8BITDO_M30_MODKIT)) {
        m30_8bitdo_modkit(&bt_data->raw_src_mappings[PAD]);
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
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_OUYA)) {
        ouya(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_8BITDO_GC)) {
        gc_diy_8bitdo(&bt_data->raw_src_mappings[PAD]);
    }
    if (atomic_test_bit(&bt_data->base.flags[PAD], BT_QUIRK_8BITDO_GBROS)) {
        gc_gbros_8bitdo(&bt_data->raw_src_mappings[PAD]);
    }
}