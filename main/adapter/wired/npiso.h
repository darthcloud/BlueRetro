/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _NPISO_H_
#define _NPISO_H_
#include "adapter/adapter.h"

void npiso_meta_init(struct generic_ctrl *ctrl_data);
void npiso_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void npiso_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data);

#endif /* _NPISO_H_ */
