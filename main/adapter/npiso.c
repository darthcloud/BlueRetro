/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "config.h"
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

static DRAM_ATTR const uint8_t npiso_mouse_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    1,       0,       1,       0,       1,     0
};

static DRAM_ATTR const struct ctrl_meta npiso_mouse_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x80, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x80, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x80},
};

static DRAM_ATTR const struct ctrl_meta npiso_trackball_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -8, .size_max = 7, .neutral = 0x00, .abs_max = 0x8},
    {.size_min = -8, .size_max = 7, .neutral = 0x00, .abs_max = 0x8, .polarity = 1},
    {.size_min = -8, .size_max = 7, .neutral = 0x00, .abs_max = 0x8},
    {.size_min = -8, .size_max = 7, .neutral = 0x00, .abs_max = 0x8, .polarity = 1},
    {.size_min = -8, .size_max = 7, .neutral = 0x00, .abs_max = 0x8},
    {.size_min = -8, .size_max = 7, .neutral = 0x00, .abs_max = 0x8},
};

struct npiso_map {
    uint16_t buttons;
} __packed;

struct npiso_mouse_map {
    uint8_t id;
    uint8_t buttons;
    uint8_t relative[2];
    int32_t raw_axes[2];
} __packed;

struct npiso_trackball_map {
    uint8_t buttons;
    uint8_t flags;
    uint8_t relative[2];
    int32_t raw_axes[2];
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

static const uint32_t npiso_mouse_mask[4] = {0x110000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t npiso_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t npiso_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(NPISO_B), 0, 0, 0,
    BIT(NPISO_Y), 0, 0, 0,
};

static const uint32_t npiso_trackball_mask[4] = {0x1907C0F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t npiso_trackball_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t npiso_trackball_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, BIT(NPISO_LD_DOWN), BIT(NPISO_LD_UP),
    BIT(NPISO_LD_LEFT), BIT(NPISO_LD_RIGHT), 0, 0,
    0, 0, 0, 0,
    BIT(NPISO_Y), 0, 0, BIT(NPISO_START),
    BIT(NPISO_B), 0, 0, 0,
};

void IRAM_ATTR npiso_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_MOUSE:
        {
            if (wired_adapter.system_id == SNES) {
                struct npiso_mouse_map *map = (struct npiso_mouse_map *)wired_data->output;

                map->id = 0xFF;
                map->buttons = 0xFE;
                map->relative[0] = 1;
                map->relative[1] = 1;
                map->raw_axes[0] = 0;
                map->raw_axes[1] = 0;
            }
            else {
                struct npiso_trackball_map *map = (struct npiso_trackball_map *)wired_data->output;

                map->buttons = 0xFF;
                map->flags = 0x2F; // Fixed switch to L & Lo
                map->relative[0] = 1;
                map->relative[1] = 1;
                map->raw_axes[0] = 0;
                map->raw_axes[1] = 0;
            }
            break;
        }
        default:
        {
            struct npiso_map *map = (struct npiso_map *)wired_data->output;

            map->buttons = 0xFFFF;
            break;
        }
    }
}

void npiso_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_MOUSE:
                    if (wired_adapter.system_id == SNES) {
                        ctrl_data[i].mask = npiso_mouse_mask;
                        ctrl_data[i].desc = npiso_mouse_desc;
                        ctrl_data[i].axes[j].meta = &npiso_mouse_axes_meta[j];
                    }
                    else {
                        ctrl_data[i].mask = npiso_trackball_mask;
                        ctrl_data[i].desc = npiso_trackball_desc;
                        ctrl_data[i].axes[j].meta = &npiso_trackball_axes_meta[j];
                    }
                    break;
                default:
                    ctrl_data[i].mask = npiso_mask;
                    ctrl_data[i].desc = npiso_desc;
                    break;
            }
        }
    }
}

void npiso_ctrl_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
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

static void npiso_mouse_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct npiso_mouse_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 4);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~npiso_mouse_btns_mask[i];
            }
            else {
                map_tmp.buttons |= npiso_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 2; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & npiso_mouse_desc[0])) {
            if (ctrl_data->axes[i].relative) {
                map_tmp.relative[npiso_mouse_axes_idx[i]] = 1;
                atomic_add(&raw_axes[npiso_mouse_axes_idx[i]], ctrl_data->axes[i].value);
            }
            else {
                map_tmp.relative[npiso_mouse_axes_idx[i]] = 0;
                raw_axes[npiso_mouse_axes_idx[i]] = ctrl_data->axes[i].value;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 8);
}

static void npiso_trackball_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct npiso_trackball_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 4);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons &= ~npiso_trackball_btns_mask[i];
            }
            else {
                map_tmp.buttons |= npiso_trackball_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 2; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & npiso_trackball_desc[0])) {
            if (ctrl_data->axes[i].relative) {
                map_tmp.relative[npiso_mouse_axes_idx[i]] = 1;
                atomic_add(&raw_axes[npiso_mouse_axes_idx[i]], ctrl_data->axes[i].value);
            }
            else {
                map_tmp.relative[npiso_mouse_axes_idx[i]] = 0;
                raw_axes[npiso_mouse_axes_idx[i]] = ctrl_data->axes[i].value;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 8);
}

void npiso_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_MOUSE:
            if (wired_adapter.system_id == SNES) {
                npiso_mouse_from_generic(ctrl_data, wired_data);
            }
            else {
                npiso_trackball_from_generic(ctrl_data, wired_data);
            }
            break;
        default:
            npiso_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
}
