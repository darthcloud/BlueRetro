/*
 * Copyright (c) 2020-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GENESIS_H_
#define _GENESIS_H_
#include "adapter/adapter.h"

void genesis_meta_init(struct wired_ctrl *ctrl_data);
void genesis_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void genesis_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void genesis_gen_turbo_mask(uint32_t index, struct wired_data *wired_data);
void genesis_twh_gen_turbo_mask(struct wired_data *wired_data);

#endif /* _GENESIS_H_ */
