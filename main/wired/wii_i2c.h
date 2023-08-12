/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _WII_I2C_H_
#define _WII_I2C_H_

#include <stdint.h>

void wii_i2c_init(uint32_t package);
void wii_i2c_port_cfg(uint16_t mask);

#endif /* _WII_I2C_H_ */
