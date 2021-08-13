/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_ATT_H_
#define _BT_ATT_H_

void bt_att_set_le_max_mtu(uint16_t le_max_mtu);
void bt_att_cfg_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);
void bt_att_hid_init(struct bt_dev *device);
void bt_att_hid_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);

#endif /* _BT_ATT_H_ */
