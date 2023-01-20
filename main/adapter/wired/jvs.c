/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "zephyr/atomic.h"
#include "tools/util.h"
#include "adapter/wired/wired.h"
#include "jvs.h"

#define JVS_AXES_MAX 2

enum {
    JVS_2 = 0,
    JVS_1,
    JVS_LD_RIGHT,
    JVS_LD_LEFT,
    JVS_LD_DOWN,
    JVS_LD_UP,
    JVS_SERVICE,
    JVS_START,
    JVS_10,
    JVS_9,
    JVS_8,
    JVS_7,
    JVS_6,
    JVS_5,
    JVS_4,
    JVS_3,
};

static DRAM_ATTR const uint8_t jvs_axes_idx[JVS_AXES_MAX] =
{
/*  AXIS_LX, AXIS_LY  */
    0,       1,
};

static DRAM_ATTR const struct ctrl_meta jvs_axes_meta[JVS_AXES_MAX] =
{
    {.size_min = -32768, .size_max = 32767, .neutral = 0x8000, .abs_max = 0x7FFF, .abs_min = 0x8000},
    {.size_min = -32768, .size_max = 32767, .neutral = 0x8000, .abs_max = 0x7FFF, .abs_min = 0x8000},
};

struct jvs_map {
    uint16_t coins;
    uint16_t buttons;
    uint16_t axes[2];
    uint8_t test;
} __packed;

static const uint32_t jvs_mask[4] = {0xBBFF0F0F, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t jvs_desc[4] = {0x0000000F, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t jvs_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(JVS_LD_LEFT), BIT(JVS_LD_RIGHT), BIT(JVS_LD_DOWN), BIT(JVS_LD_UP),
    0, 0, 0, 0,
    BIT(JVS_3), BIT(JVS_2), BIT(JVS_1), BIT(JVS_4),
    BIT(JVS_START), 0, BIT(JVS_SERVICE), 0,
    BIT(JVS_5), BIT(JVS_7), 0, BIT(JVS_9),
    BIT(JVS_6), BIT(JVS_8), 0, BIT(JVS_10),
};

void IRAM_ATTR jvs_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    struct jvs_map *map = (struct jvs_map *)wired_data->output;
    struct jvs_map *map_mask = (struct jvs_map *)wired_data->output_mask;

    map->coins = 0x0000;
    map->buttons = 0x0000;
    for (uint32_t i = 0; i < JVS_AXES_MAX; i++) {
        map->axes[jvs_axes_idx[i]] = jvs_axes_meta[i].neutral;
    }

    map_mask->buttons = 0xFFFF;
    map_mask->axes[0] = 0x0000;
    map_mask->axes[1] = 0x0000;
}

void jvs_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*4);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < JVS_AXES_MAX; j++) {
            ctrl_data[i].mask = jvs_mask;
            ctrl_data[i].desc = jvs_desc;
            ctrl_data[i].axes[j].meta = &jvs_axes_meta[j];
        }
    }
}

void jvs_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct jvs_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & generic_btns_mask[i]) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= jvs_btns_mask[i];
                map_mask &= ~jvs_btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else if (map_mask & jvs_btns_mask[i]) {
                map_tmp.buttons &= ~jvs_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_MS]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_MS]) {
            if (!atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_set_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);
            }
        }
        else {
            if (atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                uint16_t val_cpu = sys_be16_to_cpu(map_tmp.coins);
                atomic_clear_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);
                if (val_cpu < 16383) {
                    val_cpu++;
                    map_tmp.coins = sys_cpu_to_be16(val_cpu);
                }
            }
        }
    }

    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_MQ]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_MQ]) {
            map_tmp.test |= 0x80;
        }
        else {
            map_tmp.test &= ~0x80;
        }
    }

    for (uint32_t i = 0; i < JVS_AXES_MAX; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & jvs_desc[0])) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                *(uint16_t *)&map_tmp.axes[jvs_axes_idx[i]] = sys_cpu_to_be16(32767);
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                *(uint16_t *)&map_tmp.axes[jvs_axes_idx[i]] = sys_cpu_to_be16(-32768);
            }
            else {
                *(uint16_t *)&map_tmp.axes[jvs_axes_idx[i]] = sys_cpu_to_be16((uint16_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral));
            }
        }
        wired_data->cnt_mask[axis_to_btn_id(i)] = ctrl_data->axes[i].cnt_mask;
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
    printf("{\"log_type\": \"wired_output\", \"axes\": [%d, %d], \"btns\": %d, \"COINS\": %d, \"TEST\": %d}\n",
        map_tmp.axes[jvs_axes_idx[0]], map_tmp.axes[jvs_axes_idx[1]],
        map_tmp.buttons, map_tmp.coins, map_tmp.test);
#endif
}

void IRAM_ATTR jvs_gen_turbo_mask(struct wired_data *wired_data) {
    struct jvs_map *map_mask = (struct jvs_map *)wired_data->output_mask;

    map_mask->buttons = 0xFFFF;
    map_mask->axes[0] = 0x0000;
    map_mask->axes[1] = 0x0000;

    wired_gen_turbo_mask_btns16_pos(wired_data, &map_mask->buttons, jvs_btns_mask);

    for (uint32_t i = 0; i < JVS_AXES_MAX; i++) {
        uint8_t btn_id = axis_to_btn_id(i);
        uint8_t mask = wired_data->cnt_mask[btn_id] >> 1;
        if (mask) {
            if (wired_data->cnt_mask[btn_id] & 1) {
                if (!(mask & wired_data->frame_cnt)) {
                    map_mask->axes[jvs_axes_idx[i]] = jvs_axes_meta[i].neutral;
                }
            }
            else {
                if (!((mask & wired_data->frame_cnt) == mask)) {
                    map_mask->axes[jvs_axes_idx[i]] = jvs_axes_meta[i].neutral;
                }
            }
        }
    }
}
