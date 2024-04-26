/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <esp_timer.h>
#include "bluetooth/host.h"
#include "ps3.h"

static const uint8_t bt_init_magic[] = {
    0x42, 0x03, 0x00, 0x00
};

static const uint8_t ps3_config[] = {
    0x01, 0, 0x00, 0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0xff, 0x27, 0x10, 0x00, 0x32, 0xff,
    0x27, 0x10, 0x00, 0x32, 0xff, 0x27, 0x10, 0x00,
    0x32, 0xff, 0x27, 0x10, 0x00, 0x32, 0x00, 0x00,
    0x00, 0x00, 0x00
};

static void bt_hid_cmd_ps3_bt_init(struct bt_dev *device) {
    struct bt_hidp_ps3_bt_init *bt_init = (struct bt_hidp_ps3_bt_init *)bt_hci_pkt_tmp.hidp_data;

    memcpy((void *)bt_init, bt_init_magic, sizeof(*bt_init));

    bt_hid_cmd(device->acl_handle, device->ctrl_chan.dcid, BT_HIDP_SET_FE, BT_HIDP_PS3_BT_INIT, sizeof(*bt_init));
}

static void bt_hid_ps3_fb_init(struct bt_dev *device) {
    struct bt_hidp_ps3_set_conf *set_conf = (struct bt_hidp_ps3_set_conf *)bt_hci_pkt_tmp.hidp_data;
    memcpy((void *)set_conf, ps3_config, sizeof(*set_conf));
    set_conf->leds = (bt_hid_led_dev_id_map[device->ids.out_idx] << 1);

    bt_hid_cmd(device->acl_handle, device->ctrl_chan.dcid, BT_HIDP_SET_OUT, BT_HIDP_PS3_SET_CONF, sizeof(*set_conf));
}

static void bt_hid_ps3_fb_ready_callback(void *arg) {
    struct bt_dev *device = (struct bt_dev *)arg;

    if (!atomic_test_bit(&device->flags, BT_DEV_FB_DELAY)) {
        bt_hid_ps3_fb_init(device);
    }
    atomic_clear_bit(&device->flags, BT_DEV_FB_DELAY);

    esp_timer_delete(device->timer_hdl);
    device->timer_hdl = NULL;
}

static void bt_hid_ps3_init_callback(void *arg) {
    struct bt_dev *device = (struct bt_dev *)arg;

    printf("# %s\n", __FUNCTION__);

    esp_timer_delete(device->timer_hdl);
    device->timer_hdl = NULL;
    bt_hid_ps3_fb_init(device);
}

static void bt_hid_ps3_fb_delay(struct bt_dev *device, uint64_t delay_ms) {
    const esp_timer_create_args_t ps3_timer_args = {
        .callback = &bt_hid_ps3_init_callback,
        .arg = (void *)device,
        .name = "ps3_init_timer"
    };

    esp_timer_create(&ps3_timer_args, (esp_timer_handle_t *)&device->timer_hdl);
    esp_timer_start_once(device->timer_hdl, delay_ms);
}

void bt_hid_cmd_ps3_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_ps3_set_conf *set_conf = (struct bt_hidp_ps3_set_conf *)bt_hci_pkt_tmp.hidp_data;

    memcpy((void *)set_conf, report, sizeof(*set_conf));

    if (set_conf->r_rumble_pow) {
        bt_hid_cmd(device->acl_handle, device->ctrl_chan.dcid, BT_HIDP_SET_OUT, BT_HIDP_PS3_SET_CONF, sizeof(*set_conf));

        if (device->timer_hdl == NULL) {
            const esp_timer_create_args_t ps3_timer_args = {
                .callback = &bt_hid_ps3_fb_ready_callback,
                .arg = (void *)device,
                .name = "ps3_fb_timer"
            };

            esp_timer_create(&ps3_timer_args, (esp_timer_handle_t *)&device->timer_hdl);
            esp_timer_start_once(device->timer_hdl, 120000);
            atomic_set_bit(&device->flags, BT_DEV_FB_DELAY);
        }
    }
    else {
        if (!atomic_test_bit(&device->flags, BT_DEV_FB_DELAY)) {
            bt_hid_ps3_fb_init(device);
        }
        atomic_clear_bit(&device->flags, BT_DEV_FB_DELAY);
    }
}

void bt_hid_ps3_init(struct bt_dev *device) {
    printf("# %s\n", __FUNCTION__);

    /* PS3 ctrl not yet ready to RX config, delay 20ms */
    bt_hid_ps3_fb_delay(device, 1000000);
    bt_hid_cmd_ps3_bt_init(device);
}

void bt_hid_ps3_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    uint32_t hidp_data_len = len - (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
                                    + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_hidp_hdr));

    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_PS3_STATUS:
#ifdef CONFIG_BLUERETRO_ADAPTER_RUMBLE_TEST
                    struct bt_hidp_ps3_set_conf rumble;
                    memcpy(&rumble, ps3_config, sizeof(rumble));
                    if (bt_hci_acl_pkt->hidp_data[17] || bt_hci_acl_pkt->hidp_data[18]) {
                        rumble.r_rumble_pow = 0x01;
                    }
                    if (bt_hci_acl_pkt->hidp_data[17]) {
                        rumble.l_rumble_pow = bt_hci_acl_pkt->hidp_data[17];
                        rumble.l_rumble_len = 0xFF;
                    }
                    if (bt_hci_acl_pkt->hidp_data[18]) {
                        rumble.r_rumble_len = 0xFF;
                    }
                    bt_hid_cmd_ps3_set_conf(device, &rumble);
#else
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
#endif
                    break;
            }
            break;
    }
}
