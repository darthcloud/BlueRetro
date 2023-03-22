/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SATURN_H_
#define _SATURN_H_
#include "adapter/adapter.h"

void saturn_meta_init(struct wired_ctrl *ctrl_data);
void saturn_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void saturn_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void saturn_kb_id_to_scancode(uint32_t dev_id, uint8_t type, uint8_t id);
void saturn_gen_turbo_mask(struct wired_data *wired_data);

#endif /* _SATURN_H_ */
