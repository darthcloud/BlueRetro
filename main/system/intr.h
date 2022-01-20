/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INTR_H_
#define _INTR_H_

#include "soc/soc.h"
#include "esp_attr.h"
#include "freertos/xtensa_rtos.h"

int32_t IRAM_ATTR intexc_alloc_iram(uint32_t source, uint32_t intr_num, XT_INTEXC_HOOK handler);
int32_t IRAM_ATTR intexc_free_iram(uint32_t source, uint32_t intr_num);

#endif /* _INTR_H_ */
