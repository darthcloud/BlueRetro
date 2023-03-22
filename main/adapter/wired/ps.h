/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PS_H_
#define _PS_H_
#include "adapter/adapter.h"

void ps_meta_init(struct wired_ctrl *ctrl_data);
void ps_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void ps_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void ps_kb_id_to_scancode(uint32_t dev_id, uint8_t type, uint8_t id);
void ps_fb_to_generic(int32_t dev_mode, struct raw_fb *raw_fb_data, struct generic_fb *fb_data);
void ps_gen_turbo_mask(struct wired_data *wired_data);

#endif /* _PS_H_ */
