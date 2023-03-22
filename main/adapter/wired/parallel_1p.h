/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PARA_1P_H_
#define _PARA_1P_H_
#include "adapter/adapter.h"

void para_1p_meta_init(struct wired_ctrl *ctrl_data);
void para_1p_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void para_1p_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void para_1p_gen_turbo_mask(struct wired_data *wired_data);

#endif /* _PARA_1P_H_ */
