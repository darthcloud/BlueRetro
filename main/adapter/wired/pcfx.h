/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PCFX_H_
#define _PCFX_H_
#include "adapter/adapter.h"

void pcfx_meta_init(struct wired_ctrl *ctrl_data);
void pcfx_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void pcfx_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void pcfx_gen_turbo_mask(struct wired_data *wired_data);

#endif /* _PCFX_H_ */
