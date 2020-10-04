/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HIDP_PS3_H_
#define _BT_HIDP_PS3_H_

#include "hidp.h"

#define BT_HIDP_PS3_BT_INIT 0xf4
struct bt_hidp_ps3_bt_init {
    uint8_t magic[4];
} __packed;

#define BT_HIDP_PS3_STATUS 0x01
struct bt_hidp_ps3_status {
    uint8_t data[48];
} __packed;

#define BT_HIDP_PS3_SET_CONF 0x01
struct bt_hidp_ps3_set_conf {
    uint8_t tbd0;
    uint8_t r_rumble_len;
    uint8_t r_rumble_pow;
    uint8_t l_rumble_len;
    uint8_t l_rumble_pow;
    uint8_t tbd1[4];
    uint8_t leds;
    uint8_t tbd2[25];
} __packed;

void bt_hid_cmd_ps3_set_conf(struct bt_dev *device, void *report);
void bt_hid_ps3_init(struct bt_dev *device);
void bt_hid_ps3_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt);

#endif /* _BT_HIDP_PS3_H_ */
