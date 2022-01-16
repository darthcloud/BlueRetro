/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HIDP_PS_H_
#define _BT_HIDP_PS_H_

#include "hidp.h"

#define BT_HIDP_HID_STATUS 0x01
#define BT_HIDP_PS4_STATUS 0x11
#define BT_HIDP_PS5_STATUS 0x31
struct bt_hidp_ps4_status {
    uint8_t data[77];
} __packed;

#define BT_HIDP_PS4_SET_CONF 0x11
struct bt_hidp_ps4_set_conf {
    uint8_t conf0;
    uint8_t tbd0;
    uint8_t conf1;
    uint8_t tbd1[2];
    uint8_t r_rumble;
    uint8_t l_rumble;
    union {
        struct {
            uint8_t rgb[3];
            uint8_t led_on_delay;
        };
        uint32_t leds;
    };
    uint8_t led_off_delay;
    uint8_t tbd2[61];
    uint32_t crc;
} __packed;

#define BT_HIDP_PS5_SET_CONF 0x31
struct bt_hidp_ps5_set_conf {
    uint8_t conf0;
    uint8_t cmd;
    uint8_t conf1;
    uint8_t r_rumble;
    uint8_t l_rumble;
    uint8_t tbd0[4];
    uint8_t mic_led;
    uint8_t tbd1[35];
    uint32_t leds;
    uint8_t tbd2[24];
    uint32_t crc;
} __packed;

extern const uint32_t bt_ps4_ps5_led_dev_id_map[];

void bt_hid_cmd_ps_set_conf(struct bt_dev *device, void *report);
void bt_hid_ps_init(struct bt_dev *device);
void bt_hid_ps_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);

#endif /* _BT_HIDP_PS_H_ */
