/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HIDP_SW_H_
#define _BT_HIDP_SW_H_

#include "hidp.h"

#define BT_HIDP_SW_RUMBLE_IDLE 0x40400100

struct bt_hid_sw_axis_calib {
    union {
        struct {
            uint16_t rel_min;
            uint16_t neutral;
            uint16_t rel_max;
        };
        uint16_t val[3];
    };
};

struct bt_hid_sw_stick_calib {
    struct bt_hid_sw_axis_calib axes[2];
    uint16_t deadzone;
};

struct bt_hid_sw_ctrl_calib {
    struct bt_hid_sw_stick_calib sticks[2];
};

#define BT_HIDP_SW_SET_CONF 0x01
#define BT_HIDP_SW_SUBCMD_READ_INFO 0x02
#define BT_HIDP_SW_SUBCMD_SET_REP_MODE 0x03
#define BT_HIDP_SW_SUBCMD_TRIGGER_TIME 0x04
#define BT_HIDP_SW_SUBCMD_DISABLE_SHIP 0x08
#define BT_HIDP_SW_SUBCMD_READ_SPI 0x10
#define BT_HIDP_SW_SUBCMD_SET_MCU_CFG 0x21
#define BT_HIDP_SW_SUBCMD_SET_LED 0x30
#define BT_HIDP_SW_SUBCMD_ENABLE_IMU 0x40
#define BT_HIDP_SW_SUBCMD_EN_RUMBLE 0x48
struct bt_hidp_sw_conf {
    uint8_t tid;
    union {
        uint8_t rumble[8];
        uint32_t rumble32[2];
    };
    uint8_t subcmd;
    union {
        uint8_t subcmd_data[38];
        struct {
            uint16_t addr;
            uint16_t tbd;
            uint8_t len;
        };
    };
} __packed;

#define BT_HIDP_SW_SET_RUMBLE 0x10
struct bt_hidp_sw_rumble {
    uint8_t tid;
    union {
        uint8_t rumble[8];
        uint32_t rumble32[2];
    };
} __packed;

#define BT_HIDP_SW_SUBCMD_ACK 0x21
struct bt_hidp_sw_subcmd_ack {
    uint8_t tbd[13];
    uint8_t subcmd;
    uint16_t addr;
    uint16_t tbd2;
    uint8_t len;
    uint8_t data[29];
} __packed;

#define BT_HIDP_SW_STATUS_NATIVE 0x30
struct bt_hidp_sw_status_native {
    uint8_t data[11];
} __packed;

#define BT_HIDP_SW_STATUS 0x3F
struct bt_hidp_sw_status {
    uint8_t data[11];
} __packed;

void bt_hid_cmd_sw_set_conf(struct bt_dev *device, void *report);
void bt_hid_sw_get_calib(int32_t dev_id, struct bt_hid_sw_ctrl_calib **cal);
void bt_hid_sw_init(struct bt_dev *device);
void bt_hid_sw_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);

#endif /* _BT_HIDP_SW_H_ */
