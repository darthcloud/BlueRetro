/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _QUEUE_BSS_H_
#define _QUEUE_BSS_H_

typedef void * queue_bss_handle_t;

queue_bss_handle_t queue_bss_init(uint32_t item_num, uint32_t item_len);
void queue_bss_init_othercores(void);
int32_t queue_bss_enqueue(queue_bss_handle_t qhdl, uint8_t *item, uint32_t item_len);
uint8_t *queue_bss_dequeue(queue_bss_handle_t qhdl, uint32_t **item_len);
int32_t queue_bss_return(queue_bss_handle_t qhdl, uint8_t *item, uint32_t *item_len);
void queue_bss_deinit(queue_bss_handle_t qhdl);

#endif /* _QUEUE_BSS_H_ */
