/*
 * Copyright (c) 2019-2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <esp_timer.h>
#include "bluetooth/host.h"
#include "bluetooth/hci.h"
#include "tools/util.h"
#include "tests/cmds.h"
#include "bluetooth/mon.h"
#include "sw.h"

//#define SW_DISABLE_SHIP_EN
//#define SW_SET_NATIVE_EN
//#define SW_TRIGGER_TIME_EN
//#define SW_ENABLE_IMU_EN
//#define SW_SET_MCU_CFG_EN
#define SW_INIT_STATE_RETRY_MAX 10

enum {
    SW_INIT_STATE_READ_INFO = 0,
#ifdef SW_DISABLE_SHIP_EN
    SW_INIT_STATE_DISABLE_SHIP,
#endif
    SW_INIT_STATE_READ_CALIB,
#ifdef SW_SET_NATIVE_EN
    SW_INIT_STATE_SET_NATIVE,
#endif
#ifdef SW_TRIGGER_TIME_EN
    SW_INIT_STATE_TRIGGER_TIME,
#endif
#ifdef SW_ENABLE_IMU_EN
    SW_INIT_STATE_ENABLE_IMU,
#endif
    SW_INIT_STATE_EN_RUMBLE,
#ifdef SW_SET_MCU_CFG_EN
    SW_INIT_STATE_SET_MCU_CFG,
#endif
    SW_INIT_STATE_SET_LED,
};

static uint8_t calib_idx[][3] = {
    {2, 1, 0},
    {1, 0, 2},
};
static struct bt_hid_sw_ctrl_calib calib[BT_MAX_DEV] = {0};

static void bt_hid_sw_set_calib(struct bt_hid_sw_ctrl_calib *calib, uint8_t *data, uint8_t stick, uint8_t idx) {
    calib->sticks[stick].axes[0].val[calib_idx[idx][0]] = ((data[1] << 8) & 0xF00) | data[0];
    calib->sticks[stick].axes[1].val[calib_idx[idx][0]] = (data[2] << 4) | (data[1] >> 4);
    calib->sticks[stick].axes[0].val[calib_idx[idx][1]] = ((data[4] << 8) & 0xF00) | data[3];
    calib->sticks[stick].axes[1].val[calib_idx[idx][1]] = (data[5] << 4) | (data[4] >> 4);
    calib->sticks[stick].axes[0].val[calib_idx[idx][2]] = ((data[7] << 8) & 0xF00) | data[6];
    calib->sticks[stick].axes[1].val[calib_idx[idx][2]] = (data[8] << 4) | (data[7] >> 4);
}

static void bt_hid_sw_print_calib(struct bt_hid_sw_ctrl_calib *calib) {
    TESTS_CMDS_LOG("\"calib_data\": {");
    TESTS_CMDS_LOG("\"rel_min\": [%u, %u, %u, %u], ", calib->sticks[0].axes[0].rel_min, calib->sticks[0].axes[1].rel_min,
        calib->sticks[1].axes[0].rel_min, calib->sticks[1].axes[1].rel_min);
    TESTS_CMDS_LOG("\"rel_max\": [%u, %u, %u, %u], ", calib->sticks[0].axes[0].rel_max, calib->sticks[0].axes[1].rel_max,
        calib->sticks[1].axes[0].rel_max, calib->sticks[1].axes[1].rel_max);
    TESTS_CMDS_LOG("\"neutral\": [%u, %u, %u, %u], ", calib->sticks[0].axes[0].neutral, calib->sticks[0].axes[1].neutral,
        calib->sticks[1].axes[0].neutral, calib->sticks[1].axes[1].neutral);
    TESTS_CMDS_LOG("\"deadzone\": [%u, %u, %u, %u]},\n", calib->sticks[0].deadzone, calib->sticks[0].deadzone,
        calib->sticks[1].deadzone, calib->sticks[1].deadzone);
    printf("rel_min LX %03X RX %03X\n", calib->sticks[0].axes[0].rel_min, calib->sticks[1].axes[0].rel_min);
    printf("neutral LX %03X RX %03X\n", calib->sticks[0].axes[0].neutral, calib->sticks[1].axes[0].neutral);
    printf("rel_max LX %03X RX %03X\n", calib->sticks[0].axes[0].rel_max, calib->sticks[1].axes[0].rel_max);
    printf("rel_min LY %03X RY %03X\n", calib->sticks[0].axes[1].rel_min, calib->sticks[1].axes[1].rel_min);
    printf("neutral LY %03X RY %03X\n", calib->sticks[0].axes[1].neutral, calib->sticks[1].axes[1].neutral);
    printf("rel_max LY %03X RY %03X\n", calib->sticks[0].axes[1].rel_max, calib->sticks[1].axes[1].rel_max);
    printf("        LD %03X RD %03X\n", calib->sticks[0].deadzone, calib->sticks[1].deadzone);
    bt_mon_log(true, "rel_min LX %03X RX %03X\n", calib->sticks[0].axes[0].rel_min, calib->sticks[1].axes[0].rel_min);
    bt_mon_log(true, "neutral LX %03X RX %03X\n", calib->sticks[0].axes[0].neutral, calib->sticks[1].axes[0].neutral);
    bt_mon_log(true, "rel_max LX %03X RX %03X\n", calib->sticks[0].axes[0].rel_max, calib->sticks[1].axes[0].rel_max);
    bt_mon_log(true, "rel_min LY %03X RY %03X\n", calib->sticks[0].axes[1].rel_min, calib->sticks[1].axes[1].rel_min);
    bt_mon_log(true, "neutral LY %03X RY %03X\n", calib->sticks[0].axes[1].neutral, calib->sticks[1].axes[1].neutral);
    bt_mon_log(true, "rel_max LY %03X RY %03X\n", calib->sticks[0].axes[1].rel_max, calib->sticks[1].axes[1].rel_max);
    bt_mon_log(true, "        LD %03X RD %03X\n", calib->sticks[0].deadzone, calib->sticks[1].deadzone);
}

void bt_hid_cmd_sw_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_sw_conf *sw_conf = (struct bt_hidp_sw_conf *)bt_hci_pkt_tmp.hidp_data;
    uint8_t protocol = BT_HIDP_SW_SET_CONF;
    uint16_t len = sizeof(struct bt_hidp_sw_conf);

    memcpy((void *)sw_conf, report, sizeof(*sw_conf));
    sw_conf->tid = device->tid++;
    sw_conf->tid &= 0xF;


    /* 8bitdo wont rumble w/ set_conf... */
    if (((uint8_t *)report)[127] == FB_TYPE_RUMBLE) {
        protocol = BT_HIDP_SW_SET_RUMBLE;
        len = sizeof(struct bt_hidp_sw_rumble);
    }

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, protocol, len);
}

void bt_hid_sw_get_calib(int32_t dev_id, struct bt_hid_sw_ctrl_calib **cal) {
    struct bt_dev *device = NULL;
    bt_host_get_dev_from_id(dev_id, &device);

    if (device && atomic_test_bit(&device->flags, BT_DEV_CALIB_SET)) {
        *cal = &calib[dev_id];
    }
}

static void bt_hid_sw_exec_next_state(struct bt_dev *device) {
    switch(device->hid_state) {
        case SW_INIT_STATE_READ_INFO:
        {
            struct bt_hidp_sw_conf sw_conf = {
                .l_lra.val = BT_HIDP_SW_LRA_IDLE,
                .r_lra.val = BT_HIDP_SW_LRA_IDLE,
                .subcmd = BT_HIDP_SW_SUBCMD_READ_INFO,
            };
            device->tid = 0;
            bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
            break;
        }
#ifdef SW_DISABLE_SHIP_EN
        case SW_INIT_STATE_DISABLE_SHIP:
        {
            struct bt_hidp_sw_conf sw_conf = {
                .subcmd = BT_HIDP_SW_SUBCMD_DISABLE_SHIP,
            };
            bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
            break;
        }
#endif
        case SW_INIT_STATE_READ_CALIB:
        {
            struct bt_hidp_sw_conf sw_conf = {
                .subcmd = BT_HIDP_SW_SUBCMD_READ_SPI,
                .addr = 0x603D,
                .len = 18,
            };
            bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
            break;
        }
#ifdef SW_SET_NATIVE_EN
        case SW_INIT_STATE_SET_NATIVE:
        {
            struct bt_hidp_sw_conf sw_conf = {
                .subcmd = BT_HIDP_SW_SUBCMD_SET_REP_MODE,
                .subcmd_data[0] = BT_HIDP_SW_STATUS_NATIVE,
            };
            bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
            break;
        }
#endif
#ifdef SW_TRIGGER_TIME_EN
        case SW_INIT_STATE_TRIGGER_TIME:
        {
            struct bt_hidp_sw_conf sw_conf = {
                .subcmd = BT_HIDP_SW_SUBCMD_TRIGGER_TIME,
            };
            bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
            break;
        }
#endif
#ifdef SW_ENABLE_IMU_EN
        case SW_INIT_STATE_ENABLE_IMU:
        {
            struct bt_hidp_sw_conf sw_conf = {
                .subcmd = BT_HIDP_SW_SUBCMD_ENABLE_IMU,
                .subcmd_data[0] = 0x01,
            };
            bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
            break;
        }
#endif
        case SW_INIT_STATE_EN_RUMBLE:
        {
            struct bt_hidp_sw_conf sw_conf = {
                .subcmd = BT_HIDP_SW_SUBCMD_EN_RUMBLE,
                .subcmd_data[0] = 0x01,
            };
            bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
            break;
        }
#ifdef SW_SET_MCU_CFG_EN
        case SW_INIT_STATE_SET_MCU_CFG:
        {
            struct bt_hidp_sw_conf sw_conf = {
                .l_lra.val = BT_HIDP_SW_LRA_IDLE,
                .r_lra.val = BT_HIDP_SW_LRA_IDLE,
                .subcmd = BT_HIDP_SW_SUBCMD_SET_MCU_CFG,
                .subcmd_data[0] = 0x21,
            };
            bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
            break;
        }
#endif
        case SW_INIT_STATE_SET_LED:
        default:
        {
            struct bt_hidp_sw_conf sw_conf = {
                .l_lra.val = BT_HIDP_SW_LRA_IDLE,
                .r_lra.val = BT_HIDP_SW_LRA_IDLE,
                .subcmd = BT_HIDP_SW_SUBCMD_SET_LED,
                .subcmd_data[0] = bt_hid_led_dev_id_map[device->ids.out_idx],
            };
            bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
            break;
        }
    }
}

static void bt_hid_sw_init_callback(void *arg) {
    struct bt_dev *device = (struct bt_dev *)arg;

    printf("# %s\n", __FUNCTION__);
    if (device->hid_retry_cnt++ > SW_INIT_STATE_RETRY_MAX) {
        if (device->hid_state == SW_INIT_STATE_SET_LED) {
            esp_timer_delete(device->timer_hdl);
            device->timer_hdl = NULL;
            return;
        }
        device->hid_state++;
        device->hid_retry_cnt = 0;
    }
    bt_hid_sw_exec_next_state(device);

    esp_timer_start_once(device->timer_hdl, 100000);

    atomic_set_bit(&device->flags, BT_DEV_HID_INIT_DONE);
}

void bt_hid_sw_init(struct bt_dev *device) {
#ifndef CONFIG_BLUERETRO_TEST_FALLBACK_REPORT
    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
    struct bt_hidp_sw_conf *set_conf = (struct bt_hidp_sw_conf *)bt_data->base.output;

    /* Init output data for Rumble/LED feedback */
    set_conf->l_lra.val = BT_HIDP_SW_LRA_IDLE;
    set_conf->r_lra.val = BT_HIDP_SW_LRA_IDLE;
    set_conf->subcmd = BT_HIDP_SW_SUBCMD_SET_LED;
    set_conf->subcmd_data[0] = bt_hid_led_dev_id_map[bt_data->base.pids->out_idx];

    const esp_timer_create_args_t sw_timer_args = {
        .callback = &bt_hid_sw_init_callback,
        .arg = (void *)device,
        .name = "sw_init_timer"
    };
    struct bt_hid_sw_ctrl_calib *dev_calib = &calib[device->ids.id];
    printf("# %s\n", __FUNCTION__);

    memset((uint8_t *)dev_calib, 0, sizeof(*dev_calib));

    esp_timer_create(&sw_timer_args, (esp_timer_handle_t *)&device->timer_hdl);
    esp_timer_start_once(device->timer_hdl, 300000);
#endif
}

void bt_hid_sw_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    uint32_t hidp_data_len = len - (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
                                    + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_hidp_hdr));

    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_SW_SUBCMD_ACK:
                {
                    struct bt_hidp_sw_subcmd_ack *ack = (struct bt_hidp_sw_subcmd_ack *)bt_hci_acl_pkt->hidp_data;
                    switch(ack->subcmd) {
                        case BT_HIDP_SW_SUBCMD_READ_INFO:
                        {
                            esp_timer_restart(device->timer_hdl, 100000);
                            printf("# BT_HIDP_SW_SUBCMD_READ_INFO\n");
                            device->hid_state++;
                            bt_hid_sw_exec_next_state(device);
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_SET_REP_MODE:
                        {
                            esp_timer_restart(device->timer_hdl, 100000);
                            printf("# BT_HIDP_SW_SUBCMD_SET_REP_MODE\n");
                            device->hid_state++;
                            bt_hid_sw_exec_next_state(device);
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_TRIGGER_TIME:
                        {
                            esp_timer_restart(device->timer_hdl, 100000);
                            printf("# BT_HIDP_SW_SUBCMD_TRIGGER_TIME\n");
                            device->hid_state++;
                            bt_hid_sw_exec_next_state(device);
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_DISABLE_SHIP:
                        {
                            esp_timer_restart(device->timer_hdl, 100000);
                            printf("# BT_HIDP_SW_SUBCMD_DISABLE_SHIP\n");
                            device->hid_state++;
                            bt_hid_sw_exec_next_state(device);
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_READ_SPI:
                        {
                            esp_timer_restart(device->timer_hdl, 100000);
                            printf("# BT_HIDP_SW_SUBCMD_READ_SPI 0x%04X\n", ack->addr);
                            switch (ack->addr) {
                                case 0x603D:
                                {
                                    struct bt_hid_sw_ctrl_calib *dev_calib = &calib[device->ids.id];
                                    uint8_t *data = ack->data;
                                    uint8_t idx = 0;

                                    data_dump(ack->data, ack->len);

                                    if (device->ids.subtype == BT_SW_RIGHT_JOYCON) {
                                        data += 9;
                                        idx++;
                                    }
                                    for (uint32_t i = 0; i < 2; i++, idx++) {
                                        if (data[0] != 0xFF) {
                                            bt_hid_sw_set_calib(dev_calib, data, i, idx);
                                        }
                                        if (device->ids.subtype == BT_SUBTYPE_DEFAULT) {
                                            data += 9;
                                        }
                                        else {
                                            break;
                                        }
                                    }

                                    struct bt_hidp_sw_conf sw_conf = {
                                        .l_lra.val = BT_HIDP_SW_LRA_IDLE,
                                        .r_lra.val = BT_HIDP_SW_LRA_IDLE,
                                        .subcmd = BT_HIDP_SW_SUBCMD_READ_SPI,
                                        .addr = 0x6086,
                                        .len = 18,
                                    };
                                    bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
                                    break;
                                }
                                case 0x6086:
                                {
                                    struct bt_hid_sw_ctrl_calib *dev_calib = &calib[device->ids.id];
                                    uint8_t *data = ack->data;
                                    data_dump(ack->data, ack->len);

                                    dev_calib->sticks[0].deadzone = ((data[4] << 8) & 0xF00) | data[3];

                                    struct bt_hidp_sw_conf sw_conf = {
                                        .l_lra.val = BT_HIDP_SW_LRA_IDLE,
                                        .r_lra.val = BT_HIDP_SW_LRA_IDLE,
                                        .subcmd = BT_HIDP_SW_SUBCMD_READ_SPI,
                                        .addr = 0x6098,
                                        .len = 18,
                                    };
                                    bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
                                    break;
                                }
                                case 0x6098:
                                {
                                    struct bt_hid_sw_ctrl_calib *dev_calib = &calib[device->ids.id];
                                    uint8_t *data = ack->data;
                                    data_dump(ack->data, ack->len);

                                    dev_calib->sticks[1].deadzone = ((data[4] << 8) & 0xF00) | data[3];

                                    struct bt_hidp_sw_conf sw_conf = {
                                        .l_lra.val = BT_HIDP_SW_LRA_IDLE,
                                        .r_lra.val = BT_HIDP_SW_LRA_IDLE,
                                        .subcmd = BT_HIDP_SW_SUBCMD_READ_SPI,
                                        .addr = 0x8010,
                                        .len = 22,
                                    };
                                    bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
                                    break;
                                }
                                case 0x8010:
                                {
                                    struct bt_hid_sw_ctrl_calib *dev_calib = &calib[device->ids.id];
                                    uint8_t *data = ack->data;
                                    uint8_t idx = 0;

                                    data_dump(ack->data, ack->len);

                                    if (device->ids.subtype == BT_SW_RIGHT_JOYCON) {
                                        data += 11;
                                        idx++;
                                    }
                                    for (uint32_t i = 0; i < 2; i++, idx++) {
                                        if (data[0] != 0xFF) {
                                            bt_hid_sw_set_calib(dev_calib, data + 2, i, idx);
                                        }
                                        if (device->ids.subtype == BT_SUBTYPE_DEFAULT) {
                                            data += 11;
                                        }
                                        else {
                                            break;
                                        }
                                    }

                                    bt_hid_sw_print_calib(dev_calib);
                                    atomic_set_bit(&device->flags, BT_DEV_CALIB_SET);
                                    /* Force reinit once calib available */
                                    bt_type_update(device->ids.id, BT_SW, device->ids.subtype);
                                    device->hid_state++;
                                    bt_hid_sw_exec_next_state(device);
                                    break;
                                }
                            }
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_SET_MCU_CFG:
                        {
                            esp_timer_restart(device->timer_hdl, 100000);
                            printf("# BT_HIDP_SW_SUBCMD_SET_MCU_CFG\n");
                            device->hid_state++;
                            bt_hid_sw_exec_next_state(device);
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_ENABLE_IMU:
                        {
                            esp_timer_restart(device->timer_hdl, 100000);
                            printf("# BT_HIDP_SW_SUBCMD_ENABLE_IMU\n");
                            device->hid_state++;
                            bt_hid_sw_exec_next_state(device);
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_EN_RUMBLE:
                        {
                            esp_timer_restart(device->timer_hdl, 100000);
                            printf("# BT_HIDP_SW_SUBCMD_EN_RUMBLE\n");
                            device->hid_state++;
                            bt_hid_sw_exec_next_state(device);
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_SET_LED:
                        {
                            esp_timer_delete(device->timer_hdl);
                            device->timer_hdl = NULL;
                            break;
                        }
                    }
                    break;
                }
                case BT_HIDP_SW_STATUS:
                {
                    if (device->ids.report_type != BT_HIDP_SW_STATUS) {
                        bt_type_update(device->ids.id, BT_SW, device->ids.subtype);
                        device->ids.report_type = BT_HIDP_SW_STATUS;
                        printf("# %s: Report type change to %02lX\n", __FUNCTION__, device->ids.report_type);
                    }
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    break;
                }
                case BT_HIDP_SW_STATUS_NATIVE:
                {
                    if (device->ids.report_type != BT_HIDP_SW_STATUS_NATIVE) {
                        bt_type_update(device->ids.id, BT_SW, device->ids.subtype);
                        device->ids.report_type = BT_HIDP_SW_STATUS_NATIVE;
                        printf("# %s: Report type change to %02lX\n", __FUNCTION__, device->ids.report_type);
                    }
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    break;
                }
            }
            break;
    }
}
