/*
 * Copyright (c) 2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CMDS_H_
#define _CMDS_H_

#include <stdint.h>

#ifdef CONFIG_BLUERETRO_WS_CMDS
#define TESTS_CMDS_LOG(...) tests_cmds_log(__VA_ARGS__)
#else
#define TESTS_CMDS_LOG(...)
#endif

struct tests_cmds_pkt {
    uint8_t cmd;
    uint8_t handle;
    uint16_t data_len;
    uint8_t data[0];
} __packed;

char *tests_cmds_hdlr(struct tests_cmds_pkt *pkt);
void tests_cmds_log(const char * format, ...);

#endif /* _CMDS_H_ */
