/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HIDP_XBOX_H_
#define _BT_HIDP_XBOX_H_

#include "hidp.h"

#define BT_HIDP_XB1_STATUS 0x01
#define BT_HIDP_XB1_STATUS2 0x02
struct bt_hidp_xb1_status {
    uint8_t data[16];
} __packed;

struct bt_hidp_xb1_adaptive_status {
    uint8_t data[54];
} __packed;

#define BT_HIDP_XB1_RUMBLE 0x03
struct bt_hidp_xb1_rumble {
    uint8_t enable;
    uint8_t mag_lt;
    uint8_t mag_rt;
    uint8_t mag_l;
    uint8_t mag_r;
    uint8_t duration;
    uint8_t delay;
    uint8_t cnt;
} __packed;

void bt_hid_cmd_xbox_rumble(struct bt_dev *device, void *report);
void bt_hid_xbox_init(struct bt_dev *device);
void bt_hid_xbox_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);

#endif /* _BT_HIDP_XBOX_H_ */
