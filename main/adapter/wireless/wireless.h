/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _WIRELESS_H_
#define _WIRELESS_H_
#include "adapter/adapter.h"

int32_t wireless_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data);
void wireless_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data);

#endif /* _WIRELESS_H_ */
