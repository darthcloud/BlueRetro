/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GC_H_
#define _GC_H_
#include "adapter/adapter.h"

void gc_meta_init(struct wired_ctrl *ctrl_data);
void gc_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void gc_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void gc_fb_to_generic(int32_t dev_mode, struct raw_fb *raw_fb_data, struct generic_fb *fb_data);
void gc_gen_turbo_mask(struct wired_data *wired_data);

#endif /* _GC_H_ */
