/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _NSI_H_
#define _NSI_H_

#include <stdint.h>

void nsi_init(void);
void nsi_port_cfg(uint16_t mask);

#endif  /* _NSI_H_ */
