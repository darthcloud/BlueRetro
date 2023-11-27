/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "adapter/config.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "parallel_2p.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#define P1_TR_PIN 27
#define P1_TL_PIN 26
#define P1_R_PIN 23
#define P1_L_PIN 18
#define P1_D_PIN 5
#define P1_U_PIN 3

#define P2_TR_PIN 16
#define P2_TL_PIN 33
#define P2_R_PIN 25
#define P2_L_PIN 22
#define P2_D_PIN 21
#define P2_U_PIN 19

#define P1_1 P1_TL_PIN
#define P1_2 P1_TR_PIN
#define P1_LD_UP P1_U_PIN
#define P1_LD_DOWN P1_D_PIN
#define P1_LD_LEFT P1_L_PIN
#define P1_LD_RIGHT P1_R_PIN

#define P2_1 P2_TL_PIN
#define P2_2 P2_TR_PIN
#define P2_LD_UP P2_U_PIN
#define P2_LD_DOWN P2_D_PIN
#define P2_LD_LEFT P2_L_PIN
#define P2_LD_RIGHT P2_R_PIN

static const uint32_t para_2p_mask[4] = {0x00050F00, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t para_2p_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t para_2p_btns_mask[2][32] = {
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(P1_LD_LEFT), BIT(P1_LD_RIGHT), BIT(P1_LD_DOWN), BIT(P1_LD_UP),
        0, 0, 0, 0,
        BIT(P1_1), 0, BIT(P1_2), 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(P2_LD_LEFT), BIT(P2_LD_RIGHT), BIT(P2_LD_DOWN), BIT(P2_LD_UP),
        0, 0, 0, 0,
        BIT(P2_1 - 32) | 0xF0000000, 0, BIT(P2_2), 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
};

void IRAM_ATTR para_2p_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    struct para_2p_map *map1 = (struct para_2p_map *)wired_adapter.data[0].output;
    struct para_2p_map *map2 = (struct para_2p_map *)wired_adapter.data[1].output;
    struct para_2p_map *map1_mask = (struct para_2p_map *)wired_adapter.data[0].output_mask;
    struct para_2p_map *map2_mask = (struct para_2p_map *)wired_adapter.data[1].output_mask;

    map1->buttons = 0xFFFDFFFD;
    map2->buttons = 0xFFFDFFFD;
    map1_mask->buttons = 0;
    map2_mask->buttons = 0;

    map1->buttons_high = 0xFFFFFFFE;
    map2->buttons_high = 0xFFFFFFFE;
    map1_mask->buttons_high = 0;
    map2_mask->buttons_high = 0;
}

void para_2p_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        ctrl_data[i].mask = para_2p_mask;
        ctrl_data[i].desc = para_2p_desc;
    }
}

void para_2p_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    if (ctrl_data->index < 2) {
        struct para_2p_map map_tmp;
        struct para_2p_map *map1 = (struct para_2p_map *)wired_adapter.data[0].output;
        struct para_2p_map *map2 = (struct para_2p_map *)wired_adapter.data[1].output;
        struct para_2p_map *map1_mask = (struct para_2p_map *)wired_adapter.data[0].output_mask;
        struct para_2p_map *map2_mask = (struct para_2p_map *)wired_adapter.data[1].output_mask;

        memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (ctrl_data->map_mask[0] & BIT(i)) {
                if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                    if ((para_2p_btns_mask[ctrl_data->index][i] & 0xF0000000) == 0xF0000000) {
                        map_tmp.buttons_high &= ~(para_2p_btns_mask[ctrl_data->index][i] & 0x000000FF);
                    }
                    else {
                        map_tmp.buttons &= ~para_2p_btns_mask[ctrl_data->index][i];
                    }
                    wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
                }
                else {
                    if ((para_2p_btns_mask[ctrl_data->index][i] & 0xF0000000) == 0xF0000000) {
                        map_tmp.buttons_high |= para_2p_btns_mask[ctrl_data->index][i] & 0x000000FF;
                    }
                    else {
                        map_tmp.buttons |= para_2p_btns_mask[ctrl_data->index][i];
                    }
                    wired_data->cnt_mask[i] = 0;
                }
            }
        }

        memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

        GPIO.out = (map1->buttons | map1_mask->buttons) & (map2->buttons | map2_mask->buttons);
        GPIO.out1.val = (map1->buttons_high | map1_mask->buttons_high) & (map2->buttons_high | map2_mask->buttons_high);

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
        printf("{\"log_type\": \"wired_output\", \"btns\": [%ld, %ld]}\n",
            map_tmp.buttons, map_tmp.buttons_high);
#endif
    }
}

void para_2p_gen_turbo_mask(uint32_t index, struct wired_data *wired_data) {
    struct para_2p_map *map_mask = (struct para_2p_map *)wired_data->output_mask;

    memset(map_mask, 0, sizeof(*map_mask));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        uint8_t mask = wired_data->cnt_mask[i] >> 1;

        if (mask) {
            if (para_2p_btns_mask[index][i]) {
                if (wired_data->cnt_mask[i] & 1) {
                    if (!(mask & wired_data->frame_cnt)) {
                        if ((para_2p_btns_mask[index][i] & 0xF0000000) == 0xF0000000) {
                            map_mask->buttons_high |= para_2p_btns_mask[index][i];
                        }
                        else {
                            map_mask->buttons |= para_2p_btns_mask[index][i];
                        }
                    }
                }
                else {
                    if (!((mask & wired_data->frame_cnt) == mask)) {
                        if ((para_2p_btns_mask[index][i] & 0xF0000000) == 0xF0000000) {
                            map_mask->buttons_high |= para_2p_btns_mask[index][i];
                        }
                        else {
                            map_mask->buttons |= para_2p_btns_mask[index][i];
                        }
                    }
                }
            }
        }
    }
}
