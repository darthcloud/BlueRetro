/*
 * Copyright (c) 2021-2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MEMORY_CARD_H_
#define _MEMORY_CARD_H_
#include "adapter.h"

#define MC_BUFFER_SIZE (128 * 1024)
#define MC_BUFFER_BLOCK_SIZE (4 * 1024)
#define MC_BUFFER_BLOCK_CNT (MC_BUFFER_SIZE / MC_BUFFER_BLOCK_SIZE)

int32_t mc_init(void);
int32_t mc_init_mem(void);
void mc_storage_update(void);
void mc_read(uint32_t addr, uint8_t *data, uint32_t size);
void mc_write(uint32_t addr, uint8_t *data, uint32_t size);
uint8_t *mc_get_ptr(uint32_t addr);
uint32_t mc_get_state(void);
bool mc_get_ready(void);

#endif /* _MEMORY_CARD_H_ */
