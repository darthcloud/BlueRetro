/*
 * Copyright (c) 2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SRC_SW2_H_
#define _SRC_SW2_H_
#include "adapter/adapter.h"

int32_t sw2_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data);
bool sw2_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data);

#endif /* _SRC_SW2_H_ */
