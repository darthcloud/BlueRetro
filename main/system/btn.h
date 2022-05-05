/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BUTTON_H_
#define _BUTTON_H_

typedef enum {
    BOOT_BTN_DEFAULT_EVT = 0,
    BOOT_BTN_HOLD_EVT,
    BOOT_BTN_HOLD_CANCEL_EVT,
    BOOT_BTN_MAX_EVT,
} boot_btn_evt_t;

void boot_btn_init(void);
void boot_btn_set_callback(void (*cb)(void), boot_btn_evt_t evt);

#endif /* _BUTTON_H_ */
