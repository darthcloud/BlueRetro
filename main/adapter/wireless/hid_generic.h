/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _HID_GENERIC_H_
#define _HID_GENERIC_H_
#include "adapter/adapter.h"

int32_t hid_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data);
void hid_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data);

#endif /* _HID_GENERIC_H_ */
