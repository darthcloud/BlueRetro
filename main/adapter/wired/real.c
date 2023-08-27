/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "adapter/wired/wired.h"
#include "system/manager.h"
#include "real.h"

enum {
    REAL_A = 0,
    REAL_LD_LEFT,
    REAL_LD_RIGHT,
    REAL_LD_UP,
    REAL_LD_DOWN,
    REAL_M_RIGHT,
    REAL_M_MIDDLE,
    REAL_M_LEFT,
    REAL_L = 10,
    REAL_R,
    REAL_X,
    REAL_P,
    REAL_C,
    REAL_B,
};

enum {
    REAL_FS_LD_LEFT = 0,
    REAL_FS_LD_RIGHT,
    REAL_FS_LD_DOWN,
    REAL_FS_LD_UP,
    REAL_FS_C,
    REAL_FS_B,
    REAL_FS_A,
    REAL_FS_T,
    REAL_FS_R = 12,
    REAL_FS_L,
    REAL_FS_X,
    REAL_FS_P,
};

static DRAM_ATTR const uint8_t real_mouse_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    1,       0,       1,       0,       1,      0
};

static DRAM_ATTR const struct ctrl_meta real_fs_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -512, .size_max = 511, .neutral = 512, .abs_max = 511, .abs_min = 512},
    {.size_min = -512, .size_max = 511, .neutral = 512, .abs_max = 511, .abs_min = 512, .polarity = 1},
    {.size_min = -512, .size_max = 511, .neutral = 512, .abs_max = 511, .abs_min = 512}, //NA
    {.size_min = -512, .size_max = 511, .neutral = 512, .abs_max = 511, .abs_min = 512, .polarity = 1},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0xFF, .abs_min = 0}, //NA
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0xFF, .abs_min = 0}, //NA
};

static DRAM_ATTR const struct ctrl_meta real_mouse_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -512, .size_max = 511, .neutral = 0x00, .abs_max = 511, .abs_min = 512},
    {.size_min = -512, .size_max = 511, .neutral = 0x00, .abs_max = 511, .abs_min = 512, .polarity = 1},
    {.size_min = -512, .size_max = 511, .neutral = 0x00, .abs_max = 511, .abs_min = 512},
    {.size_min = -512, .size_max = 511, .neutral = 0x00, .abs_max = 511, .abs_min = 512, .polarity = 1},
    {.size_min = -512, .size_max = 511, .neutral = 0x00, .abs_max = 511, .abs_min = 512},
    {.size_min = -512, .size_max = 511, .neutral = 0x00, .abs_max = 511, .abs_min = 512},
};

struct real_map {
    uint16_t buttons;
} __packed;

struct real_fs_map {
    uint8_t ids[3];
    uint8_t axes[4];
    uint16_t buttons;
} __packed;

struct real_mouse_map {
    uint8_t id;
    uint8_t buttons;
    uint8_t relative[2];
    int32_t raw_axes[2];
} __packed;

static const uint32_t real_mask[4] = {0x33770F00, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t real_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t real_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(REAL_LD_LEFT), BIT(REAL_LD_RIGHT), BIT(REAL_LD_DOWN), BIT(REAL_LD_UP),
    0, 0, 0, 0,
    BIT(REAL_A), BIT(REAL_C), BIT(REAL_B), 0,
    BIT(REAL_P), BIT(REAL_X), 0, 0,
    BIT(REAL_L), BIT(REAL_L), 0, 0,
    BIT(REAL_R), BIT(REAL_R), 0, 0,
};

static const uint32_t real_fs_mask[4] = {0x337F0FCF, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t real_fs_desc[4] = {0x000000CF, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t real_fs_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(REAL_FS_LD_LEFT), BIT(REAL_FS_LD_RIGHT), BIT(REAL_FS_LD_DOWN), BIT(REAL_FS_LD_UP),
    0, 0, 0, 0,
    BIT(REAL_FS_A), BIT(REAL_FS_C), BIT(REAL_FS_B), BIT(REAL_FS_T),
    BIT(REAL_FS_P), BIT(REAL_FS_X), 0, 0,
    BIT(REAL_FS_L), BIT(REAL_FS_L), 0, 0,
    BIT(REAL_FS_R), BIT(REAL_FS_R), 0, 0,
};

static const uint32_t real_mouse_mask[4] = {0x190000F0, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t real_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t real_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(REAL_M_RIGHT), 0, 0, BIT(REAL_M_MIDDLE),
    BIT(REAL_M_LEFT), 0, 0, 0,
};

static void real_ctrl_special_action(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    /* Output config mode toggle GamePad/GamePadAlt */
    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_MT]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_MT]) {
            if (!atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_set_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);
            }
        }
        else {
            if (atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_clear_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);

                config.out_cfg[ctrl_data->index].dev_mode &= 0x01;
                config.out_cfg[ctrl_data->index].dev_mode ^= 0x01;
                sys_mgr_cmd(SYS_MGR_CMD_WIRED_RST);
            }
        }
    }
}

void IRAM_ATTR real_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_MOUSE:
        {
            struct real_mouse_map *map = (struct real_mouse_map *)wired_data->output;

            map->id = 0x49;
            map->buttons = 0x00;
            for (uint32_t i = 0; i < 2; i++) {
                map->raw_axes[i] = 0;
                map->relative[i] = 1;
            }
            break;
        }
        case DEV_PAD_ALT:
        {
            struct real_fs_map *map = (struct real_fs_map *)wired_data->output;

            map->ids[0] = 0x01;
            map->ids[1] = 0x7B;
            map->ids[2] = 0x08;
            map->axes[0] = 0x80;
            map->axes[1] = 0x20;
            map->axes[2] = 0x08;
            map->axes[3] = 0x02;
            map->buttons = 0x0000;
            memset(wired_data->output_mask, 0xFF, sizeof(struct real_fs_map));
            break;
        }
        default:
        {
            struct real_map *map = (struct real_map *)wired_data->output;

            map->buttons = 0x0080;
            memset(wired_data->output_mask, 0xFF, sizeof(struct real_map));
            break;
        }
    }
}

void real_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_MOUSE:
                    ctrl_data[i].mask = real_mouse_mask;
                    ctrl_data[i].desc = real_mouse_desc;
                    ctrl_data[i].axes[j].meta = &real_mouse_axes_meta[j];
                    break;
                case DEV_PAD_ALT:
                    ctrl_data[i].mask = real_fs_mask;
                    ctrl_data[i].desc = real_fs_desc;
                    ctrl_data[i].axes[j].meta = &real_fs_axes_meta[j];
                    break;
                default:
                    ctrl_data[i].mask = real_mask;
                    ctrl_data[i].desc = real_desc;
                    break;
            }
        }
    }
}

void real_ctrl_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct real_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= real_btns_mask[i];
                map_mask &= ~real_btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else if (map_mask & real_btns_mask[i]) {
                map_tmp.buttons &= ~real_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    real_ctrl_special_action(ctrl_data, wired_data);

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
    printf("{\"log_type\": \"wired_output\", \"btns\": %d}\n", map_tmp.buttons);
#endif
}

/* I didn't RE this one my self, base on : */
/* https://github.com/libretro/opera-libretro/blob/068c69ff784f2abaea69cdf1b8d3d9d39ac4826e/libopera/opera_pbus.c#L89 */
void real_fs_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct real_fs_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= real_fs_btns_mask[i];
                map_mask &= ~real_fs_btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else if (map_mask & real_fs_btns_mask[i]) {
                map_tmp.buttons &= ~real_fs_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    real_ctrl_special_action(ctrl_data, wired_data);

    for (uint32_t i = 0; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & real_fs_desc[0])) {
            uint16_t tmp = ctrl_data->axes[i].meta->neutral;

            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                tmp = 1023;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                tmp = 0;
            }
            else {
                tmp = (uint16_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }

            switch (i) {
                case 0:
                    map_tmp.axes[0] = ((tmp & 0x3FC) >> 2);
                    map_tmp.axes[1] &= ~0xC0;
                    map_tmp.axes[1] |= ((tmp & 0x003) << 6);
                    break;
                case 1:
                    map_tmp.axes[1] &= ~0x3F;
                    map_tmp.axes[1] |= ((tmp & 0x3F0) >> 4);
                    map_tmp.axes[2] &= ~0xF0;
                    map_tmp.axes[2] |= ((tmp & 0x00F) << 4);
                    break;
                case 3:
                    map_tmp.axes[2] &= ~0x0F;
                    map_tmp.axes[2] |= ((tmp & 0x3C0) >> 6);
                    map_tmp.axes[3] &= ~0xFC;
                    map_tmp.axes[3] |= ((tmp & 0x03F) << 2);
                    break;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
    printf("{\"log_type\": \"wired_output\", \"axes\": [%d, %d, %d, %d], \"btns\": %d}\n",
        map_tmp.axes[0], map_tmp.axes[1], map_tmp.axes[2], map_tmp.axes[3],
        map_tmp.buttons);
#endif
}

static void real_mouse_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct real_mouse_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 4);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= real_mouse_btns_mask[i];
            }
            else {
                map_tmp.buttons &= ~real_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 2; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & real_mouse_desc[0])) {
            if (ctrl_data->axes[i].relative) {
                map_tmp.relative[real_mouse_axes_idx[i]] = 1;
                atomic_add(&raw_axes[real_mouse_axes_idx[i]], ctrl_data->axes[i].value);
            }
            else {
                map_tmp.relative[real_mouse_axes_idx[i]] = 0;
                raw_axes[real_mouse_axes_idx[i]] = ctrl_data->axes[i].value;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 8);
}

void real_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_MOUSE:
            real_mouse_from_generic(ctrl_data, wired_data);
            break;
        case DEV_PAD_ALT:
            real_fs_from_generic(ctrl_data, wired_data);
            break;
        default:
            real_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
}

void IRAM_ATTR real_gen_turbo_mask(int32_t dev_mode, struct wired_data *wired_data) {
    const uint32_t *btns_mask = (dev_mode == DEV_PAD) ? real_btns_mask : real_fs_btns_mask;
    uint16_t *buttons = (dev_mode == DEV_PAD) ? wired_data->output_mask16 : (uint16_t *)&wired_data->output_mask[7];

    *buttons = 0xFFFF;

    wired_gen_turbo_mask_btns16_pos(wired_data, buttons, btns_mask);
}
