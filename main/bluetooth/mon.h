/*
 * Copyright (c) 2024-2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_MON_H_
#define _BT_MON_H_

#include "driver/uart.h"

#define BT_MON_CMD 2
#define BT_MON_EVT 3
#define BT_MON_ACL_TX 4
#define BT_MON_ACL_RX 5
#define BT_MON_SYS_NOTE 12

#ifdef CONFIG_BLUERETRO_BTMON_VERBOSE
#define BT_MON_LOG(...) bt_mon_log(true, __VA_ARGS__)
#else
#define BT_MON_LOG(...)
#endif

void bt_mon_init(void);
void bt_mon_tx(uint16_t opcode, uint8_t *data, uint16_t len);
void bt_mon_log(bool end, const char * format, ...);

#endif /* _BT_MON_H_ */
