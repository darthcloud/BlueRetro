/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PS4_PS5_H_
#define _PS4_PS5_H_
#include "adapter.h"

int32_t ps4_ps5_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data);
void ps4_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data);
void ps5_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data);

#endif /* _PS4_PS5_H_ */
