/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HIDP_GENERIC_H_
#define _BT_HIDP_GENERIC_H_

#include "hidp.h"

struct bt_hidp_generic_rumble {
    uint8_t state[88];
    uint8_t report_id;
    uint32_t report_size;
} __packed;

void bt_hid_cmd_generic_rumble(struct bt_dev *device, void *report);
void bt_hid_generic_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);

#endif /* _BT_HIDP_GENERIC_H_ */

