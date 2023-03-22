/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _XBOX_H_
#define _XBOX_H_
#include "adapter/adapter.h"

int32_t xbox_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data);
void xbox_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data);

#endif /* _XBOX_H_ */
