/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "adapter/wired/wired.h"
#include "pcfx.h"

enum {
    PCFX_I = 0,
    PCFX_II,
    PCFX_III,
    PCFX_IV,
    PCFX_V,
    PCFX_VI,
    PCFX_SELECT,
    PCFX_RUN,
    PCFX_LD_UP,
    PCFX_LD_RIGHT,
    PCFX_LD_DOWN,
    PCFX_LD_LEFT,
    PCFX_MODE1,
    PCFX_MODE2 = 14,
};

enum {
    PCFX_M_LEFT = 0,
    PCFX_M_RIGHT,
};

static DRAM_ATTR const uint8_t pcfx_mouse_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    1,       0,       1,       0,       1,      0
};

static DRAM_ATTR const struct ctrl_meta pcfx_mouse_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128},
};

struct pcfx_map {
    uint16_t buttons;
    uint8_t ids[0];
} __packed;

struct pcfx_mouse_map {
    uint8_t axes[2];
    uint8_t buttons;
    uint8_t id;
    uint8_t align[2];
    uint8_t relative[2];
    int32_t raw_axes[2];
} __packed;

static const uint32_t pcfx_mask[4] = {0xBB3F0F00, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t pcfx_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t pcfx_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(PCFX_LD_LEFT), BIT(PCFX_LD_RIGHT), BIT(PCFX_LD_DOWN), BIT(PCFX_LD_UP),
    0, 0, 0, 0,
    BIT(PCFX_III), BIT(PCFX_I), BIT(PCFX_II), BIT(PCFX_V),
    BIT(PCFX_RUN), BIT(PCFX_SELECT), 0, 0,
    BIT(PCFX_IV), BIT(PCFX_IV), 0, 0,
    BIT(PCFX_VI), BIT(PCFX_VI), 0, 0,
};

static const uint32_t pcfx_mouse_mask[4] = {0x110000F0, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t pcfx_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t pcfx_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(PCFX_M_RIGHT), 0, 0, 0,
    BIT(PCFX_M_LEFT), 0, 0, 0,
};

void IRAM_ATTR pcfx_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_MOUSE:
        {
            struct pcfx_mouse_map *map = (struct pcfx_mouse_map *)wired_data->output;

            map->id = 0xD0;
            map->buttons = 0x00;
            for (uint32_t i = 0; i < 2; i++) {
                map->raw_axes[i] = 0;
                map->relative[i] = 1;
            }
            break;
        }
        default:
        {
            struct pcfx_map *map = (struct pcfx_map *)wired_data->output;

            map->buttons = 0x0000;
            map->ids[0] = 0x00;
            map->ids[1] = 0xF0;

            wired_data->output_mask32[0] = 0xFFFFFFFF;
            break;
        }
    }
}

void pcfx_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_MOUSE:
                    ctrl_data[i].mask = pcfx_mouse_mask;
                    ctrl_data[i].desc = pcfx_mouse_desc;
                    ctrl_data[i].axes[j].meta = &pcfx_mouse_axes_meta[j];
                    break;
                default:
                    ctrl_data[i].mask = pcfx_mask;
                    ctrl_data[i].desc = pcfx_desc;
                    break;
            }
        }
    }
}

void pcfx_ctrl_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct pcfx_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= pcfx_btns_mask[i];
                map_mask &= ~pcfx_btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else if (map_mask & pcfx_btns_mask[i]) {
                map_tmp.buttons &= ~pcfx_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    /* Mode 1 */
    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_LJ]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_LJ]) {
            if (!atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_set_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);
            }
        }
        else {
            if (atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_clear_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);

                map_tmp.buttons ^= BIT(PCFX_MODE1);
            }
        }
    }

    /* Mode 2 */
    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_RJ]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_RJ]) {
            if (!atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2)) {
                atomic_set_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2);
            }
        }
        else {
            if (atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2)) {
                atomic_clear_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2);

                map_tmp.buttons ^= BIT(PCFX_MODE2);
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
    printf("{\"log_type\": \"wired_output\", \"btns\": %d}\n", map_tmp.buttons);
#endif
}

static void pcfx_mouse_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct pcfx_mouse_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 8);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= pcfx_mouse_btns_mask[i];
            }
            else {
                map_tmp.buttons &= ~pcfx_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 2; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & pcfx_mouse_desc[0])) {
            if (ctrl_data->axes[i].relative) {
                map_tmp.relative[pcfx_mouse_axes_idx[i]] = 1;
                atomic_add(&raw_axes[pcfx_mouse_axes_idx[i]], ctrl_data->axes[i].value);
            }
            else {
                map_tmp.relative[pcfx_mouse_axes_idx[i]] = 0;
                raw_axes[pcfx_mouse_axes_idx[i]] = ctrl_data->axes[i].value;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 8);
}

void pcfx_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_MOUSE:
            pcfx_mouse_from_generic(ctrl_data, wired_data);
            break;
        default:
            pcfx_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
}

void IRAM_ATTR pcfx_gen_turbo_mask(struct wired_data *wired_data) {
    struct pcfx_map *map_mask = (struct pcfx_map *)wired_data->output_mask;

    wired_data->output_mask32[0] = 0xFFFFFFFF;

    wired_gen_turbo_mask_btns16_pos(wired_data, &map_mask->buttons, pcfx_btns_mask);
}
