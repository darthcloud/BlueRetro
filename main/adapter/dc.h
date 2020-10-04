/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _DC_H_
#define _DC_H_
#include "adapter.h"

void dc_meta_init(int32_t dev_mode, struct generic_ctrl *ctrl_data);
void dc_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void dc_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data);
void dc_fb_to_generic(int32_t dev_mode, uint8_t *raw_fb_data, uint32_t raw_fb_len, struct generic_fb *fb_data);

#endif /* _DC_H_ */
