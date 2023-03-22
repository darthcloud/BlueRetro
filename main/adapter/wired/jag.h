/*
 * Copyright (c) 2021-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JAG_H_
#define _JAG_H_
#include "adapter/adapter.h"

void jag_meta_init(struct wired_ctrl *ctrl_data);
void jag_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void jag_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void jag_gen_turbo_mask(struct wired_data *wired_data);

#endif /* _JAG_H_ */
