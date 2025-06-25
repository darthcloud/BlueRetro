/*
 * Copyright (c) 2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HIDP_SW2_H_
#define _BT_HIDP_SW2_H_

#include "hidp.h"

#define SW2_LJC_PID 0x2066
#define SW2_RJC_PID 0x2067
#define SW2_PRO2_PID 0x2069
#define SW2_GC_PID 0x2073

#define BT_HIDP_SW2_LRA_IDLE_32 0x1e100000
#define BT_HIDP_SW2_LRA_IDLE_8 0x00
#define BT_HIDP_SW2_LRA_R_HF_FREQ 0x1e1
#define BT_HIDP_SW2_LRA_R_LF_FREQ 0x180
#define BT_HIDP_SW2_LRA_L_HF_FREQ 0xe1
#define BT_HIDP_SW2_LRA_L_LF_FREQ 0x100

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
        uint8_t tid : 4;
        uint8_t ops_cnt : 2;
        uint8_t enable : 1;
        uint8_t tbd : 1;
    };
    uint8_t val;
} sw2_fb_state_t;

struct sw2_lra_op_t {
    union {
        uint32_t val;
        struct {
            uint32_t lf_freq : 9;
            uint32_t lf_en_tone : 1;
            uint32_t lf_amp : 10;
            uint32_t hf_freq : 9;
            uint32_t hf_en_tone : 1;
            uint32_t tbd : 1;
            uint32_t enable : 1;
        };
    };
    uint8_t hf_amp;
} __packed;

struct sw2_lra_ops_t {
    sw2_fb_state_t state;
    struct sw2_lra_op_t ops[3];
} __packed;

#define BT_HIDP_SW2_OUT_ATT_HDL 0x0012
struct bt_hidp_sw2_out {
    uint8_t zero;
    struct sw2_lra_ops_t l_lra;
    struct sw2_lra_ops_t r_lra;
    uint8_t padding[9];
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
    struct sw2_lra_ops_t l_lra;
    struct sw2_lra_ops_t r_lra;
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
