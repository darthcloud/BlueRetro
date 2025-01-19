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

int bt_mon_init(int port_num, int32_t baud_rate, uint8_t data_bits,
    uint8_t stop_bits, uart_parity_t parity, uart_hw_flowcontrol_t flow_ctl);
void bt_mon_tx(uint16_t opcode, uint8_t *data, uint16_t len);
void IRAM_ATTR bt_mon_log(bool end, const char * format, ...);

#endif /* _BT_MON_H_ */
