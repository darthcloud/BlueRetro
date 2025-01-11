/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bluetooth/host.h"
#include "bluetooth/att_hid.h"
#include "generic.h"

void bt_hid_cmd_generic_rumble(struct bt_dev *device, void *report) {
    struct bt_hidp_generic_rumble *rumble_report = (struct bt_hidp_generic_rumble *)report;

    if (rumble_report->report_id && rumble_report->report_size) {
        if (atomic_test_bit(&device->flags, BT_DEV_IS_BLE)) {
            bt_att_write_hid_report(device, rumble_report->report_id, rumble_report->state, rumble_report->report_size);
        }
        else {
            memcpy(bt_hci_pkt_tmp.hidp_data, rumble_report->state, rumble_report->report_size);
            bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, rumble_report->report_id, rumble_report->report_size);
        }
    }
}

void bt_hid_generic_init(struct bt_dev *device) {
    atomic_set_bit(&device->flags, BT_DEV_HID_INIT_DONE);
}

void bt_hid_generic_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    uint32_t hidp_data_len = len - (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
                                    + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_hidp_hdr));

    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
            break;
    }
}
