/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PS_H_
#define _PS_H_
#include "adapter/adapter.h"

void ps_meta_init(struct generic_ctrl *ctrl_data);
void ps_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void ps_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data);
void ps_kb_id_to_scancode(uint8_t dev_id, uint8_t type, uint8_t id);
void ps_fb_to_generic(int32_t dev_mode, uint8_t *raw_fb_data, uint32_t raw_fb_len, struct generic_fb *fb_data);

#endif /* _PS_H_ */
