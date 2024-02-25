/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SEA_H_
#define _SEA_H_
#include "adapter/adapter.h"

/* As specified in: */
/* github.com/zwenergy/gbaHD/blob/1.4A/hdl/commTransceiver.vhd#L62-L64 */
#define GBAHD_OVERLAY 0x8000
#define GBAHD_CONFIG 0x4000
#define GBAHD_STATE 0x2000

#define GBAHD_LINE_MIN 0x2007
#define GBAHD_LINE_MAX 0x2011

#define GBAHD_CFG_SMOOTH_MASK 0x0003
#define GBAHD_CFG_GRID_MASK 0x000C

/* As specified in: */
/* github.com/zwenergy/gbaHD/blob/1.4A/hdl/padOverlay.vhd#L36-L45 */
enum {
    GBAHD_LD_UP = 0,
    GBAHD_LD_DOWN,
    GBAHD_LD_LEFT,
    GBAHD_LD_RIGHT,
    GBAHD_A,
    GBAHD_B,
    GBAHD_L,
    GBAHD_R,
    GBAHD_START,
    GBAHD_SELECT,
};

/* As specified in: */
/* github.com/zwenergy/gbaHD/blob/1.4A/hdl/commTransceiver.vhd#L168-L176 */
enum {
    GBAHD_CFG_SMOOTH2X = 0,
    GBAHD_CFG_SMOOTH4X,
    GBAHD_CFG_GRID_ACTIVE,
    GBAHD_CFG_GRID_BRIGHT,
    GBAHD_CFG_GRID_MULT,
    GBAHD_CFG_COLOR_CORRECTION,
    GBAHD_CFG_RATE,
    GBAHD_CFG_CTRL_OSD_ACTIVE,
};

/* As specified in: */
/* github.com/zwenergy/gbaHD/blob/1.4A/hdl/commTransceiver.vhd#L180 */
enum {
    GBAHD_STATE_OSD = 0,
};

struct sea_map {
    uint32_t buttons;
    uint32_t buttons_high;
    uint16_t buttons_osd;
    uint16_t gbahd_state;
    uint16_t gbahd_config;
} __packed;

void sea_meta_init(struct wired_ctrl *ctrl_data);
void sea_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void sea_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void sea_gen_turbo_mask(struct wired_data *wired_data);

#endif /* _SEA_H_ */
