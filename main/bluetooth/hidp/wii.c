/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "bluetooth/host.h"
#include "wii.h"

enum {
    WII_EXT_STATE_SET_INIT0 = 0,
    WII_EXT_STATE_SET_INIT1,
    WII_EXT_STATE_GET_TYPE,
    WII_EXT_STATE_SET_8BIT,
    WII_EXT_STATE_GET_8BIT,
};

struct bt_wii_ext_type {
    uint8_t ext_type[6];
    uint32_t subtype;
};

static const struct bt_hidp_wii_rep_mode wii_rep_conf = {
    BT_HIDP_WII_CONTINUOUS,
    BT_HIDP_WII_CORE_ACC_EXT,
};

static const struct bt_hidp_wii_wr_mem wii_ext_init0 = {
    BT_HIDP_WII_REG,
    {0xA4, 0x00, 0xF0},
    0x01,
    {0x55},
};

static const struct bt_hidp_wii_wr_mem wii_ext_init1 = {
    BT_HIDP_WII_REG,
    {0xA4, 0x00, 0xFB},
    0x01,
    {0x00},
};

static const struct bt_hidp_wii_wr_mem wii_ext_8bit = {
    BT_HIDP_WII_REG,
    {0xA4, 0x00, 0xFE},
    0x01,
    {0x03},
};

static const struct bt_hidp_wii_rd_mem wii_ext_type = {
    BT_HIDP_WII_REG,
    {0xA4, 0x00, 0xFA},
    0x0600,
};

static const struct bt_wii_ext_type bt_wii_ext_type[] = {
    {{0x00, 0x00, 0xA4, 0x20, 0x00, 0x00}, BT_WII_NUNCHUCK},
    {{0x00, 0x00, 0xA4, 0x20, 0x01, 0x01}, BT_WII_CLASSIC},
    {{0x01, 0x00, 0xA4, 0x20, 0x01, 0x01}, BT_WII_CLASSIC_PRO},
    {{0x00, 0x00, 0xA4, 0x20, 0x03, 0x01}, BT_WII_CLASSIC_8BIT},
    {{0x01, 0x00, 0xA4, 0x20, 0x03, 0x01}, BT_WII_CLASSIC_PRO_8BIT},
    {{0x00, 0x00, 0xA4, 0x20, 0x01, 0x20}, BT_WIIU_PRO},
};

static uint32_t bt_get_subtype_from_wii_ext(const uint8_t* ext_type);
static void bt_hid_cmd_wii_set_rep_mode(struct bt_dev *device, void *report);
static void bt_hid_cmd_wii_read(struct bt_dev *device, void *report);
static void bt_hid_cmd_wii_write(struct bt_dev *device, void *report);

static uint32_t bt_get_subtype_from_wii_ext(const uint8_t* ext_type) {
    for (uint32_t i = 0; i < sizeof(bt_wii_ext_type)/sizeof(*bt_wii_ext_type); i++) {
        if (memcmp(ext_type, bt_wii_ext_type[i].ext_type, sizeof(bt_wii_ext_type[0].ext_type)) == 0) {
            return bt_wii_ext_type[i].subtype;
        }
    }
    return BT_SUBTYPE_DEFAULT;
}

static void bt_wii_exec_next_state(struct bt_dev *device) {
    switch(device->hid_state) {
        case WII_EXT_STATE_SET_INIT0:
            bt_hid_cmd_wii_write(device, (void *)&wii_ext_init0);
            break;
        case WII_EXT_STATE_SET_INIT1:
            bt_hid_cmd_wii_write(device, (void *)&wii_ext_init1);
            break;
        case WII_EXT_STATE_GET_TYPE:
            bt_hid_cmd_wii_read(device, (void *)&wii_ext_type);
            break;
        case WII_EXT_STATE_SET_8BIT:
            bt_hid_cmd_wii_write(device, (void *)&wii_ext_8bit);
            break;
        case WII_EXT_STATE_GET_8BIT:
            bt_hid_cmd_wii_read(device, (void *)&wii_ext_type);
            break;
        default:
            break;
    }
}

static void bt_hid_cmd_wii_set_rep_mode(struct bt_dev *device, void *report) {
    struct bt_hidp_wii_rep_mode *wii_rep_mode = (struct bt_hidp_wii_rep_mode *)bt_hci_pkt_tmp.hidp_data;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_rep_mode, report, sizeof(*wii_rep_mode));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_WII_REP_MODE, sizeof(*wii_rep_mode));
}

static void bt_hid_cmd_wii_read(struct bt_dev *device, void *report) {
    struct bt_hidp_wii_rd_mem *wii_rd_mem = (struct bt_hidp_wii_rd_mem *)bt_hci_pkt_tmp.hidp_data;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_rd_mem, report, sizeof(*wii_rd_mem));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_WII_RD_MEM, sizeof(*wii_rd_mem));
}

static void bt_hid_cmd_wii_write(struct bt_dev *device, void *report) {
    struct bt_hidp_wii_wr_mem *wii_wr_mem = (struct bt_hidp_wii_wr_mem *)bt_hci_pkt_tmp.hidp_data;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_wr_mem, report, sizeof(*wii_wr_mem));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_WII_WR_MEM, sizeof(*wii_wr_mem));
}

void bt_hid_cmd_wii_set_feedback(struct bt_dev *device, void *report) {
    struct bt_hidp_wii_conf *wii_conf = (struct bt_hidp_wii_conf *)bt_hci_pkt_tmp.hidp_data;
    //printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_conf, report, sizeof(*wii_conf));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_WII_LED_REPORT, sizeof(*wii_conf));
}

void bt_hid_wii_init(struct bt_dev *device) {
    struct bt_hidp_wii_conf wii_conf = {.conf = (bt_hid_led_dev_id_map[device->ids.out_idx] << 4)};
    printf("# %s\n", __FUNCTION__);

    bt_hid_cmd_wii_set_feedback(device, (void *)&wii_conf);
    bt_hid_cmd_wii_set_rep_mode(device, (void *)&wii_rep_conf);
}

void bt_hid_wii_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    uint32_t hidp_data_len = len - (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
                                    + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_hidp_hdr));

    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_WII_STATUS:
                {
                    struct bt_hidp_wii_status *status = (struct bt_hidp_wii_status *)bt_hci_acl_pkt->hidp_data;
                    printf("# BT_HIDP_WII_STATUS\n");
                    if (device->ids.subtype != BT_WIIU_PRO) {
                        bt_type_update(device->ids.id, BT_WII, BT_SUBTYPE_DEFAULT);
                        if (status->flags & BT_HIDP_WII_FLAGS_EXT_CONN) {
                            device->hid_state = WII_EXT_STATE_SET_INIT0;
                            bt_hid_cmd_wii_write(device, (void *)&wii_ext_init0);
                        }
                        else {
                            bt_hid_cmd_wii_set_rep_mode(device, (void *)&wii_rep_conf);
                        }
                    }
                    break;
                }
                case BT_HIDP_WII_RD_DATA:
                {
                    struct bt_hidp_wii_rd_data *rd_data = (struct bt_hidp_wii_rd_data *)bt_hci_acl_pkt->hidp_data;
                    uint32_t subtype = bt_get_subtype_from_wii_ext(rd_data->data);
                    printf("# BT_HIDP_WII_RD_DATA\n");
                    if (subtype > BT_SUBTYPE_DEFAULT) {
                        bt_type_update(device->ids.id, BT_WII, subtype);
                    }
                    printf("# dev: %ld wii ext: %ld\n", device->ids.id, device->ids.subtype);

#ifndef CONFIG_BLUERETRO_TEST_FALLBACK_REPORT
                    if ((subtype == BT_WII_CLASSIC || subtype == BT_WII_CLASSIC_PRO) && device->hid_state < WII_EXT_STATE_SET_8BIT) {
                        device->hid_state = WII_EXT_STATE_SET_8BIT;
                        bt_wii_exec_next_state(device);
                    }
                    else
#endif
                    {
                        bt_hid_cmd_wii_set_rep_mode(device, (void *)&wii_rep_conf);
                    }
                    break;
                }
                case BT_HIDP_WII_ACK:
                {
                    struct bt_hidp_wii_ack *ack = (struct bt_hidp_wii_ack *)bt_hci_acl_pkt->hidp_data;
                    printf("# BT_HIDP_WII_ACK\n");
                    if (ack->err) {
                        printf("# dev: %ld ack err: 0x%02X\n", device->ids.id, ack->err);
                        bt_wii_exec_next_state(device);
                    }
                    else {
                        device->hid_state++;
                        switch(ack->report) {
                            case BT_HIDP_WII_WR_MEM:
                                bt_wii_exec_next_state(device);
                                break;
                        }
                    }
                    break;
                }
                case BT_HIDP_WII_CORE_ACC_EXT:
                {
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    break;
                }
            }
            break;
    }
}
