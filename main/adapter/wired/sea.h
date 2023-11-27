/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SEA_H_
#define _SEA_H_
#include "adapter/adapter.h"

enum {
    GBAHD_B = 0,
    GBAHD_A,
    GBAHD_LD_LEFT,
    GBAHD_LD_RIGHT,
    GBAHD_LD_DOWN,
    GBAHD_LD_UP,
    GBAHD_OSD,
};

struct sea_map {
    uint32_t buttons;
    uint32_t buttons_high;
    uint8_t buttons_osd;
} __packed;

void sea_meta_init(struct wired_ctrl *ctrl_data);
void sea_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void sea_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void sea_gen_turbo_mask(struct wired_data *wired_data);

#endif /* _SEA_H_ */
