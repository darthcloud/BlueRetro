/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SW_H_
#define _SW_H_
#include "adapter/adapter.h"

int32_t sw_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data);
void sw_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data);

#endif /* _SW_H_ */
