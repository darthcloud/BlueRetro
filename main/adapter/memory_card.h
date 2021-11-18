/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MEMORY_CARD_H_
#define _MEMORY_CARD_H_
#include "adapter.h"

int32_t mc_init(void);
void mc_storage_update(void);
void mc_read(uint32_t addr, uint8_t *data, uint32_t size);
void mc_write(uint32_t addr, uint8_t *data, uint32_t size);
uint8_t *mc_get_ptr(uint32_t addr);

#endif /* _MEMORY_CARD_H_ */
