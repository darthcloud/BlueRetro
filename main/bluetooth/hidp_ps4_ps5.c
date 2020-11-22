/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <esp32/rom/crc.h>
#include "host.h"
#include "hidp_ps4_ps5.h"

static const uint8_t bt_ps4_led_dev_id_map[][3] = {
    {0x00, 0x00, 0x40},
    {0x40, 0x00, 0x00},
    {0x00, 0x40, 0x00},
    {0x20, 0x00, 0x20},
    {0x02, 0x01, 0x00},
    {0x00, 0x01, 0x01},
    {0x01, 0x01, 0x01},
};

static void bt_hid_cmd_ps5_rumble_init(struct bt_dev *device) {
    struct bt_hidp_ps5_set_conf ps5_set_conf = {
        .conf0 = 0x02,
        .cmd = 0x02,
    };

    bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_conf);
}

void bt_hid_cmd_ps4_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_hci_pkt_tmp.hidp_data;

    bt_hci_pkt_tmp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_hci_pkt_tmp.hidp_hdr.protocol = BT_HIDP_PS4_SET_CONF;

    memcpy((void *)set_conf, report, sizeof(*set_conf));

    set_conf->crc = crc32_le((uint32_t)~0xFFFFFFFF, (void *)&bt_hci_pkt_tmp.hidp_hdr,
        sizeof(bt_hci_pkt_tmp.hidp_hdr) + sizeof(*set_conf) - sizeof(set_conf->crc));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_PS4_SET_CONF, sizeof(*set_conf));
}

void bt_hid_cmd_ps5_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_ps5_set_conf *set_conf = (struct bt_hidp_ps5_set_conf *)bt_hci_pkt_tmp.hidp_data;

    /* Hack for PS5 rumble, BlueRetro design dont't allow to Q 2 frames */
    if (((struct bt_hidp_ps5_set_conf *)report)->r_rumble || ((struct bt_hidp_ps5_set_conf *)report)->l_rumble) {
        bt_hid_cmd_ps5_rumble_init(device);
    }

    bt_hci_pkt_tmp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_hci_pkt_tmp.hidp_hdr.protocol = BT_HIDP_PS5_SET_CONF;

    memcpy((void *)set_conf, report, sizeof(*set_conf));

    set_conf->crc = crc32_le((uint32_t)~0xFFFFFFFF, (void *)&bt_hci_pkt_tmp.hidp_hdr,
        sizeof(bt_hci_pkt_tmp.hidp_hdr) + sizeof(*set_conf) - sizeof(set_conf->crc));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_PS5_SET_CONF, sizeof(*set_conf));
}

void bt_hid_ps4_ps5_init(struct bt_dev *device) {
    struct bt_hidp_ps4_set_conf ps4_set_conf = {
        .conf0 = 0xc4,
        .conf1 = 0x07,
    };
    struct bt_hidp_ps5_set_conf ps5_set_conf = {
        .conf0 = 0x02,
        .conf1 = 0x04,
        .conf2 = 0xFFFF,
    };

    memcpy(ps4_set_conf.leds, bt_ps4_led_dev_id_map[device->id], sizeof(ps4_set_conf.leds));

    printf("# %s\n", __FUNCTION__);

    bt_hid_cmd_ps4_set_conf(device, (void *)&ps4_set_conf);
    bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_conf);
}

void bt_hid_ps4_ps5_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_HID_STATUS:
                {
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, 9);
                    break;
                }
                case BT_HIDP_PS4_STATUS:
                {
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, sizeof(struct bt_hidp_ps4_status));
                    break;
                }
                case BT_HIDP_PS5_STATUS:
                {
                    device->type = PS5_DS;
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, sizeof(struct bt_hidp_ps4_status));
                    break;
                }
            }
            break;
    }
}
