/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _KBMON_H_
#define _KBMON_H_
#include "adapter.h"

enum {
    KBMON_TYPE_MAKE,
    KBMON_TYPE_BREAK,
};

typedef void (*kbmon_id_to_code_t)(uint32_t dev_id, uint8_t type, uint8_t id);

void kbmon_init(uint32_t dev_id, kbmon_id_to_code_t id_to_code_fnc);
void kbmon_update(uint32_t dev_id, struct wired_ctrl *ctrl_data);
int32_t kbmon_set_code(uint32_t dev_id, uint8_t *code, uint32_t len);
int32_t kbmon_get_code(uint32_t dev_id, uint8_t *code, uint32_t *len);
void kbmon_set_typematic(uint32_t dev_id, uint8_t state, uint32_t delay, uint32_t rate);

#endif /* _KBMON_H_ */
