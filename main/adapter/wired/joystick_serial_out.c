/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "adapter/kb_monitor.h"
#include "adapter/wired/wired.h"
#include "joystick_serial_out.h"

enum {
    JOYSTICK_SERIAL_OUT_SELECT = 0,
    JOYSTICK_SERIAL_OUT_L3,
    JOYSTICK_SERIAL_OUT_R3,
    JOYSTICK_SERIAL_OUT_START,
    JOYSTICK_SERIAL_OUT_D_UP,
    JOYSTICK_SERIAL_OUT_D_RIGHT,
    JOYSTICK_SERIAL_OUT_D_DOWN,
    JOYSTICK_SERIAL_OUT_D_LEFT,
    JOYSTICK_SERIAL_OUT_L2,
    JOYSTICK_SERIAL_OUT_R2,
    JOYSTICK_SERIAL_OUT_L1,
    JOYSTICK_SERIAL_OUT_R1,
    JOYSTICK_SERIAL_OUT_Y,
    JOYSTICK_SERIAL_OUT_B,
    JOYSTICK_SERIAL_OUT_A,
    JOYSTICK_SERIAL_OUT_X,
};

static DRAM_ATTR const uint8_t joystick_serial_out_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       2,       3,       4,      5
};

static DRAM_ATTR const struct ctrl_meta joystick_serial_out_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x7F, .abs_min = 0x80, .polarity = 1},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0xFF, .abs_min = 0x00},
};

struct joystick_serial_out_map {
    uint16_t buttons;
    uint8_t axes[6];
} __packed;

static const uint32_t joystick_serial_out_mask[4] = {0xBB7F0FFF, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t joystick_serial_out_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t joystick_serial_out_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(JOYSTICK_SERIAL_OUT_D_LEFT), BIT(JOYSTICK_SERIAL_OUT_D_RIGHT), BIT(JOYSTICK_SERIAL_OUT_D_DOWN), BIT(JOYSTICK_SERIAL_OUT_D_UP),
    0, 0, 0, 0,
    BIT(JOYSTICK_SERIAL_OUT_X), BIT(JOYSTICK_SERIAL_OUT_B), BIT(JOYSTICK_SERIAL_OUT_A), BIT(JOYSTICK_SERIAL_OUT_Y),
    BIT(JOYSTICK_SERIAL_OUT_START), BIT(JOYSTICK_SERIAL_OUT_SELECT), 0, 0,
    0, BIT(JOYSTICK_SERIAL_OUT_L1), 0, BIT(JOYSTICK_SERIAL_OUT_L3),
    0, BIT(JOYSTICK_SERIAL_OUT_R1), 0, BIT(JOYSTICK_SERIAL_OUT_R3),
};

void IRAM_ATTR joystick_serial_out_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    struct joystick_serial_out_map *map = (struct joystick_serial_out_map *)wired_data->output;
    struct joystick_serial_out_map *map_mask = (struct joystick_serial_out_map *)wired_data->output_mask;
    memset((void *)map, 0, sizeof(*map));
    
    map->buttons = 0xFFFF;
    map_mask->buttons = 0x0000;
    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        map->axes[joystick_serial_out_axes_idx[i]] = joystick_serial_out_axes_meta[i].neutral;
    }
    memset(map_mask->axes, 0x00, sizeof(map_mask->axes));
}

void joystick_serial_out_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            ctrl_data[i].mask = joystick_serial_out_mask;
            ctrl_data[i].desc = joystick_serial_out_desc;
            ctrl_data[i].axes[j].meta = &joystick_serial_out_axes_meta[j];
        }
    }
}

static void joystick_serial_out_ctrl_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct joystick_serial_out_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));
    
    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & generic_btns_mask[i]) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= joystick_serial_out_btns_mask[i];
                map_mask &= ~joystick_serial_out_btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else if (map_mask & joystick_serial_out_btns_mask[i]) {
                map_tmp.buttons &= ~joystick_serial_out_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & joystick_serial_out_desc[0])) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                map_tmp.axes[joystick_serial_out_axes_idx[i]] = 255;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                map_tmp.axes[joystick_serial_out_axes_idx[i]] = 0;
            }
            else {
                map_tmp.axes[joystick_serial_out_axes_idx[i]] = (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
        wired_data->cnt_mask[axis_to_btn_id(i)] = ctrl_data->axes[i].cnt_mask;
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
    printf("{\"log_type\": \"wired_output\", \"axes\": [%d, %d, %d, %d, %d, %d], \"btns\": [%d]}\n",
        map_tmp.axes[joystick_serial_out_axes_idx[0]], map_tmp.axes[joystick_serial_out_axes_idx[1]],
        map_tmp.axes[joystick_serial_out_axes_idx[2]], map_tmp.axes[joystick_serial_out_axes_idx[3]],
        map_tmp.axes[joystick_serial_out_axes_idx[4]], map_tmp.axes[joystick_serial_out_axes_idx[5]],
        map_tmp.buttons);
#endif
}

void joystick_serial_out_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    joystick_serial_out_ctrl_from_generic(ctrl_data, wired_data);
}

void IRAM_ATTR joystick_serial_out_gen_turbo_mask(struct wired_data *wired_data) {
    struct joystick_serial_out_map *map_mask = (struct joystick_serial_out_map *)wired_data->output_mask;

    map_mask->buttons = 0xFFFF;
    memset(map_mask->axes, 0x00, sizeof(map_mask->axes));

    wired_gen_turbo_mask_btns16_pos(wired_data, &map_mask->buttons, joystick_serial_out_btns_mask);
    wired_gen_turbo_mask_axes8(wired_data, map_mask->axes, ADAPTER_MAX_AXES, joystick_serial_out_axes_idx, joystick_serial_out_axes_meta);
}