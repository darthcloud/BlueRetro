/*
 * Copyright (c) 2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <esp_timer.h>
#include "bluetooth/host.h"
#include "bluetooth/hci.h"
#include "bluetooth/mon.h"
#include "bluetooth/att.h"
#include "zephyr/att.h"
#include "zephyr/gatt.h"
#include "tools/util.h"
#include "tests/cmds.h"
#include "adapter/config.h"
#include "sw2.h"

#define SW2_INIT_STATE_RETRY_MAX 10

enum {
    SW2_INIT_STATE_READ_INFO = 0,
    SW2_INIT_STATE_READ_LTK,
    SW2_INIT_STATE_SET_BDADDR,
    SW2_INIT_STATE_READ_NEW_LTK,
    SW2_INIT_STATE_READ_LEFT_FACTORY_CALIB,
    SW2_INIT_STATE_READ_RIGHT_FACTORY_CALIB,
    SW2_INIT_STATE_READ_USER_CALIB,
    SW2_INIT_STATE_SET_LED,
    SW2_INIT_STATE_EN_REPORT,
};

static struct bt_hid_sw2_ctrl_calib calib[BT_MAX_DEV] = {0};

static void bt_hid_sw2_set_calib(struct bt_hid_sw2_ctrl_calib *calib, uint8_t *data, uint8_t stick) {
    calib->sticks[stick].axes[0].neutral = ((data[1] << 8) & 0xF00) | data[0];
    calib->sticks[stick].axes[1].neutral = (data[2] << 4) | (data[1] >> 4);
    calib->sticks[stick].axes[0].rel_max = ((data[4] << 8) & 0xF00) | data[3];
    calib->sticks[stick].axes[1].rel_max = (data[5] << 4) | (data[4] >> 4);
    calib->sticks[stick].axes[0].rel_min = ((data[7] << 8) & 0xF00) | data[6];
    calib->sticks[stick].axes[1].rel_min = (data[8] << 4) | (data[7] >> 4);
}

static void bt_hid_sw2_print_calib(struct bt_hid_sw2_ctrl_calib *calib) {
    TESTS_CMDS_LOG("\"calib_data\": {");
    TESTS_CMDS_LOG("\"rel_min\": [%u, %u, %u, %u], ", calib->sticks[0].axes[0].rel_min, calib->sticks[0].axes[1].rel_min,
        calib->sticks[1].axes[0].rel_min, calib->sticks[1].axes[1].rel_min);
    TESTS_CMDS_LOG("\"rel_max\": [%u, %u, %u, %u], ", calib->sticks[0].axes[0].rel_max, calib->sticks[0].axes[1].rel_max,
        calib->sticks[1].axes[0].rel_max, calib->sticks[1].axes[1].rel_max);
    TESTS_CMDS_LOG("\"neutral\": [%u, %u, %u, %u], ", calib->sticks[0].axes[0].neutral, calib->sticks[0].axes[1].neutral,
        calib->sticks[1].axes[0].neutral, calib->sticks[1].axes[1].neutral);
    TESTS_CMDS_LOG("\"deadzone\": [%u, %u, %u, %u]},\n", calib->sticks[0].deadzone, calib->sticks[0].deadzone,
        calib->sticks[1].deadzone, calib->sticks[1].deadzone);
    printf("rel_min LX %04d RX %04d\n", calib->sticks[0].axes[0].rel_min, calib->sticks[1].axes[0].rel_min);
    printf("neutral LX %04d RX %04d\n", calib->sticks[0].axes[0].neutral, calib->sticks[1].axes[0].neutral);
    printf("rel_max LX %04d RX %04d\n", calib->sticks[0].axes[0].rel_max, calib->sticks[1].axes[0].rel_max);
    printf("rel_min LY %04d RY %04d\n", calib->sticks[0].axes[1].rel_min, calib->sticks[1].axes[1].rel_min);
    printf("neutral LY %04d RY %04d\n", calib->sticks[0].axes[1].neutral, calib->sticks[1].axes[1].neutral);
    printf("rel_max LY %04d RY %04d\n", calib->sticks[0].axes[1].rel_max, calib->sticks[1].axes[1].rel_max);
    printf("        LD %04d RD %04d\n", calib->sticks[0].deadzone, calib->sticks[1].deadzone);
    bt_mon_log(true, "rel_min LX %04d RX %04d\n", calib->sticks[0].axes[0].rel_min, calib->sticks[1].axes[0].rel_min);
    bt_mon_log(true, "neutral LX %04d RX %04d\n", calib->sticks[0].axes[0].neutral, calib->sticks[1].axes[0].neutral);
    bt_mon_log(true, "rel_max LX %04d RX %04d\n", calib->sticks[0].axes[0].rel_max, calib->sticks[1].axes[0].rel_max);
    bt_mon_log(true, "rel_min LY %04d RY %04d\n", calib->sticks[0].axes[1].rel_min, calib->sticks[1].axes[1].rel_min);
    bt_mon_log(true, "neutral LY %04d RY %04d\n", calib->sticks[0].axes[1].neutral, calib->sticks[1].axes[1].neutral);
    bt_mon_log(true, "rel_max LY %04d RY %04d\n", calib->sticks[0].axes[1].rel_max, calib->sticks[1].axes[1].rel_max);
    bt_mon_log(true, "        LD %04d RD %04d\n", calib->sticks[0].deadzone, calib->sticks[1].deadzone);
}

void bt_hid_sw2_init_rumble_gc(struct bt_data *bt_data) {
    bt_data->base.output[1] = 0x50;

    bt_data->base.output[5] = BT_HIDP_SW2_CMD_SET_LED;
    bt_data->base.output[6] = BT_HIDP_SW2_REQ_TYPE_REQ;
    bt_data->base.output[7] = BT_HIDP_SW2_REQ_INT_BLE;
    bt_data->base.output[8] = BT_HIDP_SW2_SUBCMD_SET_LED;
    bt_data->base.output[10] = 0x08;
    bt_data->base.output[13] = bt_hid_led_dev_id_map[bt_data->base.pids->out_idx];
}

void bt_hid_sw2_init_rumble_pro2(struct bt_data *bt_data) {
    bt_data->base.output[1] = 0x50;
    bt_data->base.output[2] = 0xe1;
    bt_data->base.output[4] = 0x10;
    bt_data->base.output[5] = 0x1e;

    bt_data->base.output[17] = 0x50;
    bt_data->base.output[18] = 0xe1;
    bt_data->base.output[20] = 0x10;
    bt_data->base.output[21] = 0x1e;

    bt_data->base.output[33] = BT_HIDP_SW2_CMD_SET_LED;
    bt_data->base.output[34] = BT_HIDP_SW2_REQ_TYPE_REQ;
    bt_data->base.output[35] = BT_HIDP_SW2_REQ_INT_BLE;
    bt_data->base.output[36] = BT_HIDP_SW2_SUBCMD_SET_LED;
    bt_data->base.output[38] = 0x08;
    bt_data->base.output[41] = bt_hid_led_dev_id_map[bt_data->base.pids->out_idx];
}

void bt_hid_cmd_sw2_out(struct bt_dev *device, void *report) {
    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
    if (bt_data) {
        switch (bt_data->base.pid) {
            case SW2_PRO2_PID:
                bt_att_cmd_write_cmd(device->acl_handle, BT_HIDP_SW2_OUT_CMD_ATT_HDL, (uint8_t *)report, 41);
                bt_data->base.output[17] &= 0xF0;
                bt_data->base.output[17] |= device->tid & 0xF;
                break;
            case SW2_GC_PID:
                bt_att_cmd_write_cmd(device->acl_handle, BT_HIDP_SW2_OUT_CMD_ATT_HDL, (uint8_t *)report, 21);
                break;
        }

        bt_data->base.output[1] &= 0xF0;
        bt_data->base.output[1] |= device->tid++ & 0xF;
    }
}

void bt_hid_sw2_get_calib(int32_t dev_id, struct bt_hid_sw2_ctrl_calib **cal) {
    struct bt_dev *device = NULL;
    bt_host_get_dev_from_id(dev_id, &device);

    if (device && atomic_test_bit(&device->flags, BT_DEV_CALIB_SET)) {
        *cal = &calib[dev_id];
    }
}

static void bt_hid_sw2_exec_next_state(struct bt_dev *device) {
    switch(device->hid_state) {
        case SW2_INIT_STATE_READ_INFO:
        {
            uint8_t read_info[] = {
                BT_HIDP_SW2_CMD_READ_SPI,
                BT_HIDP_SW2_REQ_TYPE_REQ,
                BT_HIDP_SW2_REQ_INT_BLE,
                BT_HIDP_SW2_SUBCMD_READ_SPI,
                0x00, 0x08, 0x00, 0x00, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x30, 0x01, 0x00
            };
            bt_att_cmd_write_cmd(device->acl_handle, BT_HIDP_SW2_CMD_ATT_HDL, read_info, sizeof(read_info));
            break;
        }
        case SW2_INIT_STATE_SET_BDADDR:
        {
            uint8_t set_bdaddr[] = {
                BT_HIDP_SW2_CMD_PAIRING,
                BT_HIDP_SW2_REQ_TYPE_REQ,
                BT_HIDP_SW2_REQ_INT_BLE,
                BT_HIDP_SW2_SUBCMD_PAIRING_STEP1,
                0x00, 0x0e, 0x00, 0x00, 0x00, 0x02,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            };
            bt_addr_le_t bdaddr;
            bt_hci_get_le_local_addr(&bdaddr);
            memcpy(&set_bdaddr[10], bdaddr.a.val, sizeof(bdaddr.a.val));
            memcpy(&set_bdaddr[16], bdaddr.a.val, sizeof(bdaddr.a.val));
            set_bdaddr[16]--;
            bt_att_cmd_write_cmd(device->acl_handle, BT_HIDP_SW2_CMD_ATT_HDL, set_bdaddr, sizeof(set_bdaddr));
            break;
        }
        case SW2_INIT_STATE_READ_LTK:
        case SW2_INIT_STATE_READ_NEW_LTK:
        {
            uint8_t read_ltk[] = {
                BT_HIDP_SW2_CMD_READ_SPI,
                BT_HIDP_SW2_REQ_TYPE_REQ,
                BT_HIDP_SW2_REQ_INT_BLE,
                BT_HIDP_SW2_SUBCMD_READ_SPI,
                0x00, 0x08, 0x00, 0x00,
                0x10, /* Read len */
                0x7e, 0x00, 0x00,
                0x1a, 0xa0, 0x1f, 0x00, /* LTK offset in SPI flash */
            };
            bt_att_cmd_write_cmd(device->acl_handle, BT_HIDP_SW2_CMD_ATT_HDL, read_ltk, sizeof(read_ltk));
            break;
        }
        case SW2_INIT_STATE_READ_LEFT_FACTORY_CALIB:
        {
            uint8_t read_calib[] = {
                BT_HIDP_SW2_CMD_READ_SPI,
                BT_HIDP_SW2_REQ_TYPE_REQ,
                BT_HIDP_SW2_REQ_INT_BLE,
                BT_HIDP_SW2_SUBCMD_READ_SPI,
                0x00, 0x08, 0x00, 0x00,
                0x40, /* Read len */
                0x7e, 0x00, 0x00,
                0x80, 0x30, 0x01, 0x00, /* Left Factory Calib addr */
            };
            bt_att_cmd_write_cmd(device->acl_handle, BT_HIDP_SW2_CMD_ATT_HDL, read_calib, sizeof(read_calib));
            break;
        }
        case SW2_INIT_STATE_READ_RIGHT_FACTORY_CALIB:
        {
            uint8_t read_calib[] = {
                BT_HIDP_SW2_CMD_READ_SPI,
                BT_HIDP_SW2_REQ_TYPE_REQ,
                BT_HIDP_SW2_REQ_INT_BLE,
                BT_HIDP_SW2_SUBCMD_READ_SPI,
                0x00, 0x08, 0x00, 0x00,
                0x40, /* Read len */
                0x7e, 0x00, 0x00,
                0xc0, 0x30, 0x01, 0x00, /* Right Factory Calib addr */
            };
            bt_att_cmd_write_cmd(device->acl_handle, BT_HIDP_SW2_CMD_ATT_HDL, read_calib, sizeof(read_calib));
            break;
        }
        case SW2_INIT_STATE_READ_USER_CALIB:
        {
            uint8_t read_calib[] = {
                BT_HIDP_SW2_CMD_READ_SPI,
                BT_HIDP_SW2_REQ_TYPE_REQ,
                BT_HIDP_SW2_REQ_INT_BLE,
                BT_HIDP_SW2_SUBCMD_READ_SPI,
                0x00, 0x08, 0x00, 0x00,
                0x40, /* Read len */
                0x7e, 0x00, 0x00,
                0x40, 0xc0, 0x1f, 0x00, /* User Calib addr */
            };
            bt_att_cmd_write_cmd(device->acl_handle, BT_HIDP_SW2_CMD_ATT_HDL, read_calib, sizeof(read_calib));
            break;
        }
        case SW2_INIT_STATE_SET_LED:
        {
            uint8_t led[] = {
                BT_HIDP_SW2_CMD_SET_LED,
                BT_HIDP_SW2_REQ_TYPE_REQ,
                BT_HIDP_SW2_REQ_INT_BLE,
                BT_HIDP_SW2_SUBCMD_SET_LED,
                0x00, 0x08, 0x00, 0x00,
                bt_hid_led_dev_id_map[device->ids.out_idx],
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            };
            bt_att_cmd_write_cmd(device->acl_handle, BT_HIDP_SW2_CMD_ATT_HDL, led, sizeof(led));
            break;
        }
        case SW2_INIT_STATE_EN_REPORT:
        default:
        {
            uint16_t data = BT_GATT_CCC_NOTIFY;
            bt_att_cmd_write_req(device->acl_handle, 0x000b, (uint8_t *)&data, sizeof(data));
            data = 0;
            bt_att_cmd_write_req(device->acl_handle, 0x001b, (uint8_t *)&data, sizeof(data));
            break;
        }
    }
}

void bt_hid_sw2_init(struct bt_dev *device) {
    /* enable cmds rsp */
    uint16_t data = BT_GATT_CCC_NOTIFY;
    bt_att_cmd_write_req(device->acl_handle, 0x001b, (uint8_t *)&data, sizeof(data));

    bt_hid_sw2_exec_next_state(device);

    atomic_set_bit(&device->flags, BT_DEV_HID_INIT_DONE);
}

void bt_hid_sw2_hdlr(struct bt_dev *device, uint16_t att_handle, uint8_t *data, uint32_t len) {
    switch (att_handle) {
        case BT_HIDP_SW2_REPORT_TYPE1_ATT_HDL:
            bt_host_bridge(device, 1, data, len);
            struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
            if (bt_data && bt_data->base.pid != SW2_GC_PID
                    && config.out_cfg[device->ids.out_idx].acc_mode & ACC_RUMBLE) {
                bt_hid_cmd_sw2_out(device, bt_data->base.output);
            }
            break;
        case BT_HIDP_SW2_REPORT_TYPE2_ATT_HDL:
            bt_host_bridge(device, 2, data, len);
            break;
        case BT_HIDP_SW2_ACK_ATT_HDL:
        {
            struct bt_hidp_sw2_ack *ack = (struct bt_hidp_sw2_ack *)data;
            switch (ack->cmd) {
                case BT_HIDP_SW2_CMD_READ_SPI:
                    switch (device->hid_state) {
                        case SW2_INIT_STATE_READ_INFO:
                        {
                            struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
                            if (bt_data) {
                                bt_data->base.vid = *(uint16_t *)&ack->value[30];
                                bt_data->base.pid = *(uint16_t *)&ack->value[32];

                                printf("%s: VID: 0x%04X PID: 0x%04X\n", __FUNCTION__, bt_data->base.vid, bt_data->base.pid);
                                bt_mon_log(true, "%s: VID: 0x%04X PID: 0x%04X\n", __FUNCTION__, bt_data->base.vid, bt_data->base.pid);

                                /* Init output data for Rumble/LED feedback */
                                switch (bt_data->base.pid) {
                                    case SW2_LJC_PID:
                                    case SW2_RJC_PID:
                                    case SW2_PRO2_PID:
                                        bt_hid_sw2_init_rumble_pro2(bt_data);
                                        break;
                                    case SW2_GC_PID:
                                        bt_hid_sw2_init_rumble_gc(bt_data);
                                        break;
                                    default:
                                        printf("# Unknown pid : %04X\n", bt_data->base.pid);
                                        break;
                                }
                            }
                            break;
                        }
                        case SW2_INIT_STATE_READ_LTK:
                        {
                            struct bt_smp_encrypt_info encrypt_info = {0};

                            printf("%s: LTK: ", __FUNCTION__);
                            for (uint32_t i = 0; i < 16; i++) {
                                printf("%02X ", ack->value[12 + i]);
                            }
                            printf("\n");
                            if (bt_host_load_le_ltk(&device->le_remote_bdaddr, &encrypt_info, NULL) == 0) {
                                if (memcmp(encrypt_info.ltk, &ack->value[12], sizeof(encrypt_info.ltk)) == 0) {
                                    /* LTK match, skip pairing */
                                    device->hid_state += 2;
                                }
                            }
                            break;
                        }
                        case SW2_INIT_STATE_READ_NEW_LTK:
                            printf("%s: NEW LTK: ", __FUNCTION__);
                            for (uint32_t i = 0; i < 16; i++) {
                                printf("%02X ", ack->value[12 + i]);
                            }
                            printf("\n");
                            bt_host_store_le_ltk(&device->le_remote_bdaddr, (struct bt_smp_encrypt_info *)&ack->value[12]);
                            break;
                        case SW2_INIT_STATE_READ_LEFT_FACTORY_CALIB:
                        {
                            struct bt_hid_sw2_ctrl_calib *dev_calib = &calib[device->ids.id];
                            uint8_t *data = &ack->value[52];
                            if (data[0] != 0xFF) {
                                bt_hid_sw2_set_calib(dev_calib, data, 0);
                            }
                            break;
                        }
                        case SW2_INIT_STATE_READ_RIGHT_FACTORY_CALIB:
                        {
                            struct bt_hid_sw2_ctrl_calib *dev_calib = &calib[device->ids.id];
                            uint8_t *data = &ack->value[52];
                            if (data[0] != 0xFF) {
                                bt_hid_sw2_set_calib(dev_calib, data, 1);
                            }
                            bt_hid_sw2_print_calib(dev_calib);
                            break;
                        }
                        case SW2_INIT_STATE_READ_USER_CALIB:
                        {
                            struct bt_hid_sw2_ctrl_calib *dev_calib = &calib[device->ids.id];
                            uint8_t *data = &ack->value[14];
                            if (data[0] != 0xFF) {
                                bt_hid_sw2_set_calib(dev_calib, data, 0);
                            }
                            data = &ack->value[46];
                            if (data[0] != 0xFF) {
                                bt_hid_sw2_set_calib(dev_calib, data, 1);
                            }
                            bt_hid_sw2_print_calib(dev_calib);
                            atomic_set_bit(&device->flags, BT_DEV_CALIB_SET);
                            /* Force reinit once calib available */
                            bt_type_update(device->ids.id, BT_SW2, device->ids.subtype);
                            break;
                        }
                    }
                    device->hid_state++;
                    bt_hid_sw2_exec_next_state(device);
                    break;
                case BT_HIDP_SW2_CMD_SET_LED:
                    printf("# BT_HIDP_SW2_CMD_SET_LED\n");
                    device->hid_state++;
                    bt_hid_sw2_exec_next_state(device);
                    break;
                case BT_HIDP_SW2_CMD_PAIRING:
                    switch(ack->subcmd) {
                        case BT_HIDP_SW2_SUBCMD_PAIRING_STEP1:
                        {
                            uint8_t pair2[] = {
                                BT_HIDP_SW2_CMD_PAIRING,
                                BT_HIDP_SW2_REQ_TYPE_REQ,
                                BT_HIDP_SW2_REQ_INT_BLE,
                                BT_HIDP_SW2_SUBCMD_PAIRING_STEP2,
                                0x00, 0x11, 0x00, 0x00, 0x00, 0xea, 0xbd, 0x47, 0x13, 0x89, 0x35, 0x42, 0xc6, 0x79, 0xee, 0x07, 0xf2, 0x53, 0x2c, 0x6c, 0x31
                            };
                            bt_att_cmd_write_cmd(device->acl_handle, BT_HIDP_SW2_CMD_ATT_HDL, pair2, sizeof(pair2));
                            break;
                        }
                        case BT_HIDP_SW2_SUBCMD_PAIRING_STEP2:
                        {
                            uint8_t pair3[] = {
                                BT_HIDP_SW2_CMD_PAIRING,
                                BT_HIDP_SW2_REQ_TYPE_REQ,
                                BT_HIDP_SW2_REQ_INT_BLE,
                                BT_HIDP_SW2_SUBCMD_PAIRING_STEP3,
                                0x00, 0x11, 0x00, 0x00, 0x00, 0x40, 0xb0, 0x8a, 0x5f, 0xcd, 0x1f, 0x9b, 0x41, 0x12, 0x5c, 0xac, 0xc6, 0x3f, 0x38, 0xa0, 0x73
                            };
                            bt_att_cmd_write_cmd(device->acl_handle, BT_HIDP_SW2_CMD_ATT_HDL, pair3, sizeof(pair3));
                            break;
                        }
                        case BT_HIDP_SW2_SUBCMD_PAIRING_STEP3:
                        {
                            uint8_t pair4[] = {
                                BT_HIDP_SW2_CMD_PAIRING,
                                BT_HIDP_SW2_REQ_TYPE_REQ,
                                BT_HIDP_SW2_REQ_INT_BLE,
                                BT_HIDP_SW2_SUBCMD_PAIRING_STEP4,
                                0x00, 0x01, 0x00, 0x00, 0x00
                            };
                            bt_att_cmd_write_cmd(device->acl_handle, BT_HIDP_SW2_CMD_ATT_HDL, pair4, sizeof(pair4));
                            break;
                        }
                        case BT_HIDP_SW2_SUBCMD_PAIRING_STEP4:
                        {
                            device->hid_state++;
                            bt_hid_sw2_exec_next_state(device);
                            break;
                        }
                    }
                    break;
            }
            break;
        }
        default:
            printf("%s: unknown handle %04X\n", __FUNCTION__, att_handle);
            break;
    }
}
