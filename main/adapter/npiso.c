/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "util.h"
#include "npiso.h"

enum {
    NPISO_LD_RIGHT = 0,
    NPISO_LD_LEFT,
    NPISO_LD_DOWN,
    NPISO_LD_UP,
    NPISO_START,
    NPISO_SELECT,
    NPISO_Y,
    NPISO_B,
    NPISO_R = 12,
    NPISO_L,
    NPISO_X,
    NPISO_A,
};

struct npiso_map {
    uint16_t buttons;
} __packed;

static const uint32_t npiso_mask[4] = {0x113F0F00, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t npiso_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t npiso_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(NPISO_LD_LEFT), BIT(NPISO_LD_RIGHT), BIT(NPISO_LD_DOWN), BIT(NPISO_LD_UP),
    0, 0, 0, 0,
    BIT(NPISO_Y), BIT(NPISO_A), BIT(NPISO_B), BIT(NPISO_X),
    BIT(NPISO_START), BIT(NPISO_SELECT), 0, 0,
    BIT(NPISO_L), 0, 0, 0,
    BIT(NPISO_R), 0, 0, 0,
};

void npiso_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    struct npiso_map *map = (struct npiso_map *)wired_data->output;

    map->buttons = 0xFFFF;
}

void npiso_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        ctrl_data[i].mask = npiso_mask;
        ctrl_data[i].desc = npiso_desc;
    }
}

void npiso_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct npiso_map map_tmp;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~npiso_btns_mask[i];
            }
            else {
                map_tmp.buttons |= npiso_btns_mask[i];
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}
