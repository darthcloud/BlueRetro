/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JOYSTICK_SERIAL_OUT_H_
#define _JOYSTICK_SERIAL_OUT_H_
#include "adapter/adapter.h"

void joystick_serial_out_meta_init(struct wired_ctrl *ctrl_data);
void joystick_serial_out_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void joystick_serial_out_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void joystick_serial_out_gen_turbo_mask(struct wired_data *wired_data);

#endif /* _JOYSTICK_SERIAL_OUT_H_ */
