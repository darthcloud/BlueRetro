/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ADAPTER_DEBUG_H_
#define _ADAPTER_DEBUG_H_

void adapter_debug_wireless_print(struct wireless_ctrl *ctrl_input);
void adapter_debug_wired_print(struct wired_ctrl *ctrl_input);
void adapter_debug_injector_init(void);

#endif /* _ADAPTER_DEBUG_H_ */
