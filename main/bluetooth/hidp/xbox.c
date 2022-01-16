/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "bluetooth/host.h"
#include "bluetooth/att.h"
#include "xbox.h"

void bt_hid_cmd_xbox_rumble(struct bt_dev *device, void *report) {
    if (atomic_test_bit(&device->flags, BT_DEV_IS_BLE)) {
        bt_att_write_hid_report(device, BT_HIDP_XB1_RUMBLE, report, sizeof(struct bt_hidp_xb1_rumble));
    }
    else {
        struct bt_hidp_xb1_rumble *rumble = (struct bt_hidp_xb1_rumble *)bt_hci_pkt_tmp.hidp_data;

        memcpy((void *)rumble, report, sizeof(*rumble));

        bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_XB1_RUMBLE, sizeof(*rumble));
    }
}

void bt_hid_xbox_init(struct bt_dev *device) {
    printf("# %s\n", __FUNCTION__);
}

void bt_hid_xbox_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_XB1_STATUS:
                {
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data,
                        device->ids.subtype == BT_SUBTYPE_DEFAULT ? sizeof(struct bt_hidp_xb1_status) : sizeof(struct bt_hidp_xb1_adaptive_status));
                    break;
                }
                case BT_HIDP_XB1_STATUS2:
                {
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, 1);
                    break;
                }
            }
            break;
    }
}
