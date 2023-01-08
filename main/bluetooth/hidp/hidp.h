/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HIDP_H_
#define _BT_HIDP_H_

#define BT_HIDP_SET_OUT        0x52
#define BT_HIDP_SET_FE         0x53
#define BT_HIDP_DATA_IN        0xa1
#define BT_HIDP_DATA_OUT       0xa2

struct bt_dev;
struct bt_hci_pkt;

struct bt_hidp_hdr {
    uint8_t hdr;
    uint8_t protocol;
} __packed;

extern const uint8_t bt_hid_led_dev_id_map[];

void bt_hid_set_type_flags_from_name(struct bt_dev *device, const char* name);
void bt_hid_init(struct bt_dev *device);
void bt_hid_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);
void bt_hid_feedback(struct bt_dev *device, void *report);
void bt_hid_cmd(uint16_t handle, uint16_t cid, uint8_t hidp_hdr, uint8_t protocol, uint16_t len);

#endif /* _BT_HIDP_H_ */
