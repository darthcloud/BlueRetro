/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SYS_MANAGER_H_
#define _SYS_MANAGER_H_ 

void sys_mgr_reset(void);
void sys_mgr_inquiry_toggle(void);
void sys_mgr_power_on(void);
void sys_mgr_power_off(void);
int32_t sys_mgr_get_power(void);
int32_t sys_mgr_get_boot_btn(void);
void sys_mgr_factory_reset(void);
void sys_mgr_deep_sleep(void);
void sys_mgr_init(void);

#endif /* _SYS_MANAGER_H_ */
