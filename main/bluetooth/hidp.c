/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "host.h"
#include "hidp_generic.h"
#include "hidp_ps3.h"
#include "hidp_wii.h"
#include "hidp_ps4.h"
#include "hidp_xb1.h"
#include "hidp_sw.h"

typedef void (*bt_hid_init_t)(struct bt_dev *device);
typedef void (*bt_hid_hdlr_t)(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt);
typedef void (*bt_hid_cmd_t)(struct bt_dev *device, void *report);

const uint8_t bt_hid_led_dev_id_map[] = {
    0x1, 0x2, 0x4, 0x8, 0x3, 0x6, 0xC
};

static const bt_hid_init_t bt_hid_init_list[BT_MAX] = {
    NULL, /* HID_GENERIC */
    bt_hid_ps3_init, /* PS3_DS3 */
    bt_hid_wii_init, /* WII_CORE */
    bt_hid_wii_init, /* WII_NUNCHUCK */
    bt_hid_wii_init, /* WII_CLASSIC */
    bt_hid_wii_init, /* WIIU_PRO */
    bt_hid_ps4_init, /* PS4_DS4 */
    bt_hid_xb1_init, /* XB1_S */
    bt_hid_xb1_init, /* XB1_ADAPTIVE */
    bt_hid_sw_init, /* SW */
};

static const bt_hid_hdlr_t bt_hid_hdlr_list[BT_MAX] = {
    bt_hid_generic_hdlr, /* HID_GENERIC */
    bt_hid_ps3_hdlr, /* PS3_DS3 */
    bt_hid_wii_hdlr, /* WII_CORE */
    bt_hid_wii_hdlr, /* WII_NUNCHUCK */
    bt_hid_wii_hdlr, /* WII_CLASSIC */
    bt_hid_wii_hdlr, /* WIIU_PRO */
    bt_hid_ps4_hdlr, /* PS4_DS4 */
    bt_hid_xb1_hdlr, /* XB1_S */
    bt_hid_xb1_hdlr, /* XB1_ADAPTIVE */
    bt_hid_sw_hdlr, /* SW */
};

static const bt_hid_cmd_t bt_hid_feedback_list[BT_MAX] = {
    NULL, /* HID_GENERIC */
    bt_hid_cmd_ps3_set_conf, /* PS3_DS3 */
    bt_hid_cmd_wii_set_feedback, /* WII_CORE */
    bt_hid_cmd_wii_set_feedback, /* WII_NUNCHUCK */
    bt_hid_cmd_wii_set_feedback, /* WII_CLASSIC */
    bt_hid_cmd_wii_set_feedback, /* WIIU_PRO */
    bt_hid_cmd_ps4_set_conf, /* PS4_DS4 */
    bt_hid_cmd_xb1_rumble, /* XB1_S */
    bt_hid_cmd_xb1_rumble, /* XB1_ADAPTIVE */
    bt_hid_cmd_sw_set_conf, /* SW */
};

void bt_hid_init(struct bt_dev *device) {
    if (device->type > BT_NONE && bt_hid_init_list[device->type]) {
        bt_hid_init_list[device->type](device);
    }
}

void bt_hid_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    if (device->type > BT_NONE && bt_hid_hdlr_list[device->type]) {
        bt_hid_hdlr_list[device->type](device, bt_hci_acl_pkt);
    }
}

void bt_hid_feedback(struct bt_dev *device, void *report) {
    if (device->type > BT_NONE && bt_hid_feedback_list[device->type]) {
        bt_hid_feedback_list[device->type](device, report);
    }
}

void bt_hid_cmd(uint16_t handle, uint16_t cid, uint8_t hidp_hdr, uint8_t protocol, uint16_t len) {
    uint16_t packet_len = (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
        + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_hidp_hdr) + len);

    bt_hci_pkt_tmp.h4_hdr.type = BT_HCI_H4_TYPE_ACL;

    bt_hci_pkt_tmp.acl_hdr.handle = bt_acl_handle_pack(handle, 0x2);
    bt_hci_pkt_tmp.acl_hdr.len = packet_len - BT_HCI_H4_HDR_SIZE - BT_HCI_ACL_HDR_SIZE;

    bt_hci_pkt_tmp.l2cap_hdr.len = bt_hci_pkt_tmp.acl_hdr.len - sizeof(bt_hci_pkt_tmp.l2cap_hdr);
    bt_hci_pkt_tmp.l2cap_hdr.cid = cid;

    bt_hci_pkt_tmp.hidp_hdr.hdr = hidp_hdr;
    bt_hci_pkt_tmp.hidp_hdr.protocol = protocol;

    bt_host_txq_add((uint8_t *)&bt_hci_pkt_tmp, packet_len);
}
