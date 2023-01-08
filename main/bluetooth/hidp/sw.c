/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "bluetooth/host.h"
#include "tools/util.h"
#include "sw.h"

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
    printf("# rel_min LX %03X RX %03X\n", calib->sticks[0].axes[0].rel_min, calib->sticks[1].axes[0].rel_min);
    printf("# neutral LX %03X RX %03X\n", calib->sticks[0].axes[0].neutral, calib->sticks[1].axes[0].neutral);
    printf("# rel_max LX %03X RX %03X\n", calib->sticks[0].axes[0].rel_max, calib->sticks[1].axes[0].rel_max);
    printf("# rel_min LY %03X RY %03X\n", calib->sticks[0].axes[1].rel_min, calib->sticks[1].axes[1].rel_min);
    printf("# neutral LY %03X RY %03X\n", calib->sticks[0].axes[1].neutral, calib->sticks[1].axes[1].neutral);
    printf("# rel_max LY %03X RY %03X\n", calib->sticks[0].axes[1].rel_max, calib->sticks[1].axes[1].rel_max);
    printf("#         LD %03X RD %03X\n", calib->sticks[0].deadzone, calib->sticks[1].deadzone);
}

void bt_hid_cmd_sw_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_sw_conf *sw_conf = (struct bt_hidp_sw_conf *)bt_hci_pkt_tmp.hidp_data;

    memcpy((void *)sw_conf, report, sizeof(*sw_conf));
    sw_conf->tid = device->tid++;
    sw_conf->tid &= 0xF;

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_SW_SET_CONF, sizeof(*sw_conf));
}

void bt_hid_sw_get_calib(int32_t dev_id, struct bt_hid_sw_ctrl_calib **cal) {
    *cal = &calib[dev_id];
}

void bt_hid_sw_init(struct bt_dev *device) {
#ifndef CONFIG_BLUERETRO_TEST_FALLBACK_REPORT
    struct bt_hid_sw_ctrl_calib *dev_calib = &calib[device->ids.id];
    struct bt_hidp_sw_conf sw_conf = {
        .subcmd = BT_HIDP_SW_SUBCMD_SET_LED,
        .subcmd_data[0] = bt_hid_led_dev_id_map[device->ids.out_idx],
    };
    printf("# %s\n", __FUNCTION__);

    memset((uint8_t *)dev_calib, 0, sizeof(*dev_calib));

    bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
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
                        case BT_HIDP_SW_SUBCMD_SET_REP_MODE:
                        {
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_READ_SPI:
                        {
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

                                    device->hid_state = 1;
                                    bt_hid_sw_print_calib(dev_calib);

                                    struct bt_hidp_sw_conf sw_conf = {
                                        .subcmd = BT_HIDP_SW_SUBCMD_SET_REP_MODE,
                                        .subcmd_data[0] = BT_HIDP_SW_STATUS_NATIVE,
                                    };
                                    bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);

                                    /* Enable report stall monitoring */
                                    if (device->ids.subtype != BT_SW_HYPERKIN_ADMIRAL) {
                                        atomic_set_bit(&device->flags, BT_DEV_REPORT_MON);
                                    }

                                    break;
                                }
                            }
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_SET_LED:
                        {
                            if (!device->hid_state) {
                                struct bt_hidp_sw_conf sw_conf = {
                                    .subcmd = BT_HIDP_SW_SUBCMD_EN_RUMBLE,
                                    .subcmd_data[0] = 0x01,
                                };
                                bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
                            }
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_EN_RUMBLE:
                        {
                            if (!device->hid_state) {
                                struct bt_hidp_sw_conf sw_conf = {
                                    .subcmd = BT_HIDP_SW_SUBCMD_READ_SPI,
                                    .addr = 0x603D,
                                    .len = 18,
                                };
                                bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
                            }
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
                    }
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    break;
                }
                case BT_HIDP_SW_STATUS_NATIVE:
                {
                    if (device->ids.report_type != BT_HIDP_SW_STATUS_NATIVE) {
                        bt_type_update(device->ids.id, BT_SW, device->ids.subtype);
                        device->ids.report_type = BT_HIDP_SW_STATUS_NATIVE;
                    }
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    break;
                }
            }
            break;
    }
}
