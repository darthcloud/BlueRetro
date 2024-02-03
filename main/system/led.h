/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LED_H_
#define _LED_H_

#include <stdint.h>

void err_led_init(uint32_t package);
void err_led_cfg_update(void);
void err_led_set(void);
void err_led_clear(void);
void err_led_pulse(void);
uint32_t err_led_get_pin(void);

#endif /* _LED_H_ */
