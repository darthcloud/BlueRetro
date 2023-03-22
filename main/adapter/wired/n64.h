/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _N64_H_
#define _N64_H_
#include "adapter/adapter.h"

void n64_meta_init(struct wired_ctrl *ctrl_data);
void n64_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void n64_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void n64_fb_to_generic(int32_t dev_mode, struct raw_fb *raw_fb_data, struct generic_fb *fb_data);
void n64_gen_turbo_mask(struct wired_data *wired_data);

#endif /* _N64_H_ */
