/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <esp32/rom/crc.h>
#include <esp_timer.h>
#include "bluetooth/host.h"
#include "ps.h"

const uint32_t bt_ps4_ps5_led_dev_id_map[] = {
    0xFF0000, /* Blue */
    0x0000FF, /* Red */
    0x00FF00, /* Green */
    0xFF00FF, /* Pink */
    0xFFFF00, /* Cyan */
    0x0080FF, /* Orange */
    0x00FFFF, /* Yellow */
    0xFF0080, /* Purple */
};

static void bt_hid_cmd_ps5_set_conf(struct bt_dev *device, void *report);

static void bt_hid_cmd_ps4_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_hci_pkt_tmp.hidp_data;

    bt_hci_pkt_tmp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_hci_pkt_tmp.hidp_hdr.protocol = BT_HIDP_PS4_SET_CONF;

    memcpy((void *)set_conf, report, sizeof(*set_conf));

    set_conf->crc = crc32_le((uint32_t)~0xFFFFFFFF, (void *)&bt_hci_pkt_tmp.hidp_hdr,
        sizeof(bt_hci_pkt_tmp.hidp_hdr) + sizeof(*set_conf) - sizeof(set_conf->crc));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_PS4_SET_CONF, sizeof(*set_conf));
}

static void bt_hid_cmd_ps5_rumble_init(struct bt_dev *device) {
    struct bt_hidp_ps5_set_conf ps5_set_conf = {
        .conf0 = 0x02,
        .cmd = 0x02,
    };

    bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_conf);
}

static void bt_hid_ps5_init_callback(void *arg) {
    struct bt_dev *device = (struct bt_dev *)arg;
    struct bt_hidp_ps5_set_conf ps5_set_conf = {
        .conf0 = 0x02,
        .conf1 = 0x08,
    };
    struct bt_hidp_ps5_set_conf ps5_set_led = {
        .conf0 = 0x02,
        .conf1 = 0xF6,
    };
    ps5_set_led.leds = bt_ps4_ps5_led_dev_id_map[device->ids.out_idx];
    printf("# %s\n", __FUNCTION__);

    bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_conf);
    bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_led);

    esp_timer_delete(device->timer_hdl);
    device->timer_hdl = NULL;
}

static void bt_hid_cmd_ps5_set_conf(struct bt_dev *device, void *report) {
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

void bt_hid_cmd_ps_set_conf(struct bt_dev *device, void *report) {
    switch (device->ids.subtype) {
        case BT_PS5_DS:
            bt_hid_cmd_ps5_set_conf(device, report);
            break;
        default:
            bt_hid_cmd_ps4_set_conf(device, report);
            break;
    }
}

void bt_hid_ps_init(struct bt_dev *device) {
#ifndef CONFIG_BLUERETRO_TEST_FALLBACK_REPORT
    const esp_timer_create_args_t ps5_timer_args = {
        .callback = &bt_hid_ps5_init_callback,
        .arg = (void *)device,
        .name = "ps5_init_timer"
    };
    struct bt_hidp_ps4_set_conf ps4_set_conf = {
        .conf0 = 0xc0,
        .conf1 = 0x07,
    };
    ps4_set_conf.leds = bt_ps4_ps5_led_dev_id_map[device->ids.out_idx];

    printf("# %s\n", __FUNCTION__);

    esp_timer_create(&ps5_timer_args, (esp_timer_handle_t *)&device->timer_hdl);
    esp_timer_start_once(device->timer_hdl, 1000000);
    bt_hid_cmd_ps4_set_conf(device, (void *)&ps4_set_conf);
#endif
}

void bt_hid_ps_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    uint32_t hidp_data_len = len - (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
                                    + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_hidp_hdr));

    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_HID_STATUS:
                    if (device->ids.subtype != BT_SUBTYPE_DEFAULT) {
                        bt_type_update(device->ids.id, BT_PS, BT_SUBTYPE_DEFAULT);
                    }
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    break;
                case BT_HIDP_PS4_STATUS:
                    if (device->ids.report_type != BT_HIDP_PS4_STATUS) {
                        bt_type_update(device->ids.id, BT_PS, BT_SUBTYPE_DEFAULT);
                        device->ids.report_type = BT_HIDP_PS4_STATUS;
                    }
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    break;
                case BT_HIDP_PS5_STATUS:
                    if (device->ids.report_type != BT_HIDP_PS5_STATUS) {
                        bt_type_update(device->ids.id, BT_PS, BT_PS5_DS);
                        device->ids.report_type = BT_HIDP_PS5_STATUS;
                    }
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    break;
            }
            break;
    }
}
