/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LED_H_
#define _LED_H_

void err_led_init(void);
void err_led_set(void);
void err_led_clear(void);
void err_led_pulse(void);
void d1m_led_set(void);
void d1m_led_clear(void);

#endif /* _LED_H_ */
