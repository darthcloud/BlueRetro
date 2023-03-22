/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _REAL_H_
#define _REAL_H_
#include "adapter/adapter.h"

void real_meta_init(struct wired_ctrl *ctrl_data);
void real_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void real_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void real_gen_turbo_mask(int32_t dev_mode, struct wired_data *wired_data);

#endif /* _REAL_H_ */
