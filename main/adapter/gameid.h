/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GAMEID_H_
#define _GAMEID_H_

#include "adapter/adapter.h"

int32_t gid_update(struct raw_fb *fb_data);
char *gid_get(void);

#endif /* _GAMEID_H_ */
