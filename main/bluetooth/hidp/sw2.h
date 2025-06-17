/*
 * Copyright (c) 2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HIDP_SW2_H_
#define _BT_HIDP_SW2_H_

#include "hidp.h"

#define BT_HIDP_SW2_LRA_IDLE 0x40400100
#define BT_HIDP_SW2_LRA_R_HF_FREQ 0x028
#define BT_HIDP_SW2_LRA_R_LF_FREQ 0x70
#define BT_HIDP_SW2_LRA_L_HF_FREQ 0x060
#define BT_HIDP_SW2_LRA_L_LF_FREQ 0x70

struct bt_hid_sw2_axis_calib {
    union {
        struct {
            uint16_t rel_min;
            uint16_t neutral;
            uint16_t rel_max;
        };
        uint16_t val[3];
    };
};

struct bt_hid_sw2_stick_calib {
    struct bt_hid_sw2_axis_calib axes[2];
    uint16_t deadzone;
};

struct bt_hid_sw2_ctrl_calib {
    struct bt_hid_sw2_stick_calib sticks[2];
};

#define BT_HIDP_SW2_CMD_READ_SPI 0x02
#define BT_HIDP_SW2_SUBCMD_READ_SPI 0x04

#define BT_HIDP_SW2_CMD_SET_LED 0x09
#define BT_HIDP_SW2_SUBCMD_SET_LED 0x07

#define BT_HIDP_SW2_CMD_PAIRING 0x15
#define BT_HIDP_SW2_SUBCMD_PAIRING_STEP1 0x01
#define BT_HIDP_SW2_SUBCMD_PAIRING_STEP2 0x04
#define BT_HIDP_SW2_SUBCMD_PAIRING_STEP3 0x02
#define BT_HIDP_SW2_SUBCMD_PAIRING_STEP4 0x03

typedef union {
    struct {
        uint8_t val[5];
        uint8_t tbd[11];
    };
    uint8_t val16[16];
} sw2_lra_t;

#define BT_HIDP_SW2_OUT_ATT_HDL 0x0012
struct bt_hidp_sw2_out {
    uint8_t pad;
    sw2_lra_t l_lra;
    sw2_lra_t r_lra;
    uint8_t pad2[9];
} __packed;

#define BT_HIDP_SW2_REQ_TYPE_REQ 0x91
#define BT_HIDP_SW2_REQ_TYPE_RSP 0x01
#define BT_HIDP_SW2_REQ_TYPE_ERR 0x00
#define BT_HIDP_SW2_REQ_INT_USB 0x00
#define BT_HIDP_SW2_REQ_INT_BLE 0x01
#define BT_HIDP_SW2_CMD_ATT_HDL 0x0014
struct bt_hidp_sw2_cmd {
    uint8_t cmd;
    uint8_t type;
    uint8_t interface;
    uint8_t subcmd;
    uint8_t value[0];
} __packed;

#define BT_HIDP_SW2_OUT_CMD_ATT_HDL 0x0016
struct bt_hidp_sw2_out_cmd {
    uint8_t pad;
    sw2_lra_t l_lra;
    sw2_lra_t r_lra;
    uint8_t cmd;
    uint8_t type;
    uint8_t interface;
    uint8_t subcmd;
    uint8_t value[0];
} __packed;

#define BT_HIDP_SW2_REPORT_TYPE1 0x01
#define BT_HIDP_SW2_REPORT_TYPE1_ATT_HDL 0x000A
struct bt_hidp_sw2_status_native {
    uint8_t data[63];
} __packed;

#define BT_HIDP_SW2_REPORT_TYPE2 0x02
#define BT_HIDP_SW2_REPORT_TYPE2_ATT_HDL 0x000E
struct bt_hidp_sw2_status {
    uint8_t data[63];
} __packed;

#define BT_HIDP_SW2_ACK_ATT_HDL 0x001A
struct bt_hidp_sw2_ack {
    uint8_t cmd;
    uint8_t type;
    uint8_t interface;
    uint8_t subcmd;
    uint8_t value[0];
} __packed;

void bt_hid_cmd_sw2_out(struct bt_dev *device, void *report);
void bt_hid_sw2_get_calib(int32_t dev_id, struct bt_hid_sw2_ctrl_calib **cal);
void bt_hid_sw2_init(struct bt_dev *device);
void bt_hid_sw2_hdlr(struct bt_dev *device, uint16_t att_handle, uint8_t *data, uint32_t len);

#endif /* _BT_HIDP_SW2_H_ */
