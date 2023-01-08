/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "host.h"
#include "hci.h"
#include "att.h"
#include "att_hid.h"
#include "zephyr/uuid.h"
#include "zephyr/att.h"
#include "zephyr/gatt.h"
#include "adapter/hid_parser.h"

#define HID_MAX_REPORT 5

enum {
    BT_ATT_HID_DEVICE_NAME = 0,
    BT_ATT_HID_DISCOVERY,
    BT_ATT_HID_REPORT_MAP,
    BT_ATT_HID_REPORT_REF,
    BT_ATT_HID_REPORT_CFG,
};

struct bt_att_report_ref {
    uint8_t report_id;
    uint8_t report_type;
} __packed;

struct bt_att_group_data_uuid16 {
    uint16_t start_handle;
    uint16_t end_handle;
    uint16_t uuid;
} __packed;

struct bt_att_hid_report {
    uint8_t id;
    uint8_t type;
    uint16_t report_hdl;
    uint16_t cfg_hdl;
    uint16_t ref_hdl;
};

struct bt_att_hid {
    uint16_t start_hdl;
    uint16_t end_hdl;
    uint16_t map_hdl;
    uint8_t report_cnt;
    uint8_t report_found;
    uint8_t report_idx;
    struct bt_att_hid_report reports[HID_MAX_REPORT];
};

static struct bt_att_hid att_hid[7] = {0};

static int32_t bt_att_is_report_required(struct bt_att_hid_report *att_hid, struct bt_data *bt_data) {
    if (att_hid->type == 0x01) {
        for (uint32_t i = 0; i < REPORT_MAX; i++) {
            if (att_hid->id == bt_data->reports[i].id) {
                return 1;
            }
        }
    }
    return 0;
}

static uint16_t bt_att_get_report_handle(struct bt_att_hid *hid_data, uint8_t report_id) {
    for (uint32_t i = 0; i < HID_MAX_REPORT; i++) {
        if (hid_data->reports[i].id == report_id) {
            return hid_data->reports[i].report_hdl;
        }
    }
    return 0;
}

void bt_att_hid_init(struct bt_dev *device) {
    struct bt_att_hid *hid_data = &att_hid[device->ids.id];
    memset((uint8_t *)hid_data, 0, sizeof(*hid_data));
    bt_att_cmd_mtu_req(device->acl_handle, bt_att_get_le_max_mtu());
}

void bt_att_write_hid_report(struct bt_dev *device, uint8_t report_id, uint8_t *data, uint32_t len) {
    uint16_t att_handle = bt_att_get_report_handle(&att_hid[device->ids.id], report_id);

    if (att_handle) {
        bt_att_cmd_write_req(device->acl_handle, att_handle, data, len);
    }
}

void bt_att_hid_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
    struct bt_att_hid *hid_data = &att_hid[device->ids.id];
    uint32_t att_len = len - (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE + sizeof(struct bt_l2cap_hdr));

    switch (bt_hci_acl_pkt->att_hdr.code) {
        case BT_ATT_OP_ERROR_RSP:
        {
            struct bt_att_error_rsp *error_rsp = (struct bt_att_error_rsp *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_ERROR_RSP\n");

            switch (error_rsp->request) {
                case BT_ATT_OP_READ_GROUP_REQ:
                    printf("# BT_ATT_OP_READ_GROUP_REQ\n");
                    bt_att_cmd_find_info_req(device->acl_handle, hid_data->start_hdl, hid_data->end_hdl);
                    break;
                case BT_ATT_OP_FIND_INFO_REQ:
                    printf("# BT_ATT_OP_FIND_INFO_REQ\n");
                    device->hid_state = BT_ATT_HID_REPORT_MAP;
                    bt_att_cmd_read_req(device->acl_handle, hid_data->map_hdl);
                    break;
            }
            break;
        }
        case BT_ATT_OP_MTU_RSP:
        {
            struct bt_att_exchange_mtu_rsp *mtu_rsp = (struct bt_att_exchange_mtu_rsp *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_MTU_RSP\n");

            device->mtu = mtu_rsp->mtu;

            device->hid_state = BT_ATT_HID_DEVICE_NAME;
            bt_att_cmd_read_type_req_uuid16(device->acl_handle, 0x0001, 0xFFFF, BT_UUID_GAP_DEVICE_NAME);
            break;
        }
        case BT_ATT_OP_FIND_INFO_RSP:
        {
            struct bt_att_find_info_rsp *find_info_rsp = (struct bt_att_find_info_rsp *)bt_hci_acl_pkt->att_data;
            uint16_t last_handle;
            printf("# BT_ATT_OP_FIND_INFO_RSP\n");

            if (find_info_rsp->format == BT_ATT_INFO_16) {
                struct bt_att_info_16 *info = (struct bt_att_info_16 *)find_info_rsp->info;
                const uint32_t elem_cnt = (att_len - 2)/sizeof(*info);

                last_handle = info[elem_cnt - 1].handle;

                for (uint32_t i = 0; i < elem_cnt; i++) {
                    switch (info[i].uuid) {
                        case BT_UUID_GATT_PRIMARY:
                            hid_data->report_cnt = 0;
                            break;
                        case BT_UUID_GATT_CHRC:
                            if (hid_data->report_found) {
                                hid_data->report_cnt++;
                                if (hid_data->report_cnt == HID_MAX_REPORT) {
                                    goto find_info_rsp_end;
                                }
                            }
                            hid_data->report_found = 0;
                            break;
                        case BT_UUID_HIDS_REPORT:
                            hid_data->reports[hid_data->report_cnt].report_hdl = info[i].handle;
                            hid_data->report_found = 1;
                            break;
                        case BT_UUID_GATT_CCC:
                            hid_data->reports[hid_data->report_cnt].cfg_hdl = info[i].handle;
                            break;
                        case BT_UUID_HIDS_REPORT_REF:
                            hid_data->reports[hid_data->report_cnt].ref_hdl = info[i].handle;
                            break;
                        case BT_UUID_HIDS_REPORT_MAP:
                            hid_data->map_hdl = info[i].handle;
                            break;
                        case BT_UUID_HIDS_INFO:
                        case BT_UUID_HIDS_CTRL_POINT:
                            break;
                    }
                }
            }
            else {
                struct bt_att_info_128 *info = (struct bt_att_info_128 *)find_info_rsp->info;
                const uint32_t elem_cnt = (att_len - 2)/sizeof(*info);

                last_handle = info[elem_cnt - 1].handle;

                for (uint32_t i = 0; i < elem_cnt; i++) {
                    uint16_t *uuid = (uint16_t *)&info[i].uuid[12];
                    switch (*uuid) {
                        case BT_UUID_GATT_PRIMARY:
                            hid_data->report_cnt = 0;
                            break;
                        case BT_UUID_GATT_CHRC:
                            if (hid_data->report_found) {
                                hid_data->report_cnt++;
                                if (hid_data->report_cnt == HID_MAX_REPORT) {
                                    goto find_info_rsp_end;
                                }
                            }
                            hid_data->report_found = 0;
                            break;
                        case BT_UUID_HIDS_REPORT:
                            hid_data->reports[hid_data->report_cnt].report_hdl = info[i].handle;
                            hid_data->report_found = 1;
                            break;
                        case BT_UUID_GATT_CCC:
                            hid_data->reports[hid_data->report_cnt].cfg_hdl = info[i].handle;
                            break;
                        case BT_UUID_HIDS_REPORT_REF:
                            hid_data->reports[hid_data->report_cnt].ref_hdl = info[i].handle;
                            break;
                        case BT_UUID_HIDS_REPORT_MAP:
                            hid_data->map_hdl = info[i].handle;
                            break;
                        case BT_UUID_HIDS_INFO:
                        case BT_UUID_HIDS_CTRL_POINT:
                            break;
                    }
                }
            }

find_info_rsp_end:
            if (last_handle < hid_data->end_hdl) {
                bt_att_cmd_find_info_req(device->acl_handle, last_handle + 1, hid_data->end_hdl);
            }
            else {
                device->hid_state = BT_ATT_HID_REPORT_MAP;
                bt_att_cmd_read_req(device->acl_handle, hid_data->map_hdl);
            }
            break;
        }
        case BT_ATT_OP_READ_TYPE_RSP:
        {
            struct bt_att_read_type_rsp *read_type_rsp = (struct bt_att_read_type_rsp *)bt_hci_acl_pkt->att_data;
            uint8_t rsp_len = read_type_rsp->len - sizeof(read_type_rsp->data[0].handle);
            printf("# BT_ATT_OP_READ_TYPE_RSP\n");

            switch (device->hid_state) {
                case BT_ATT_HID_DEVICE_NAME:
                {
                    char device_name[32] = {0};

                    if (rsp_len > 31) {
                        rsp_len = 31;
                    }
                    memcpy(device_name, read_type_rsp->data[0].value, rsp_len);
                    bt_hid_set_type_flags_from_name(device, device_name);
                    printf("# dev: %ld type: %ld %s\n", device->ids.id, device->ids.type, device_name);

                    device->hid_state = BT_ATT_HID_DISCOVERY;
                    bt_att_cmd_read_group_req_uuid16(device->acl_handle, 0x0001, BT_UUID_GATT_PRIMARY);
                    break;
                }
            }
            break;
        }
        case BT_ATT_OP_READ_RSP:
        {
            struct bt_att_read_rsp *read_rsp = (struct bt_att_read_rsp *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_READ_RSP\n");

            switch (device->hid_state) {
                case BT_ATT_HID_REPORT_MAP:
                {
                    if (bt_data->base.sdp_data == NULL) {
                        bt_data->base.sdp_data = malloc(BT_SDP_DATA_SIZE);
                        if (bt_data->base.sdp_data == NULL) {
                            printf("# dev: %ld Failed to alloc report memory\n", device->ids.id);
                            break;
                        }
                    }
                    memcpy(bt_data->base.sdp_data + bt_data->base.sdp_len, read_rsp->value, att_len - 1);
                    bt_data->base.sdp_len += att_len - 1;

                    if (att_len == device->mtu) {
                        bt_att_cmd_read_blob_req(device->acl_handle, hid_data->map_hdl, bt_data->base.sdp_len);
                    }
                    else {
                        hid_parser(bt_data, bt_data->base.sdp_data, bt_data->base.sdp_len);
                        if (bt_data->base.sdp_data) {
                            free(bt_data->base.sdp_data);
                            bt_data->base.sdp_data = NULL;
                        }
                        if (hid_data->reports[hid_data->report_idx].ref_hdl) {
                            device->hid_state = BT_ATT_HID_REPORT_REF;
                            bt_att_cmd_read_req(device->acl_handle, hid_data->reports[hid_data->report_idx].ref_hdl);
                        }
                        else {
                            printf("# dev: %ld No report found!!\n", device->ids.id);
                        }
                    }
                    break;
                }
                case BT_ATT_HID_REPORT_REF:
                {
                    struct bt_att_report_ref *report_ref = (struct bt_att_report_ref *)read_rsp->value;

                    hid_data->reports[hid_data->report_idx].id = report_ref->report_id;
                    hid_data->reports[hid_data->report_idx++].type = report_ref->report_type;
                    if (hid_data->reports[hid_data->report_idx].ref_hdl) {
                        bt_att_cmd_read_req(device->acl_handle, hid_data->reports[hid_data->report_idx].ref_hdl);
                    }
                    else {
                        uint32_t i = 0;
                        device->hid_state = BT_ATT_HID_REPORT_CFG;

                        for (i = 0; i < HID_MAX_REPORT; i++) {
                            if (bt_att_is_report_required(&hid_data->reports[i], bt_data)) {
                                uint16_t data = 0x0001;
                                bt_att_cmd_write_req(device->acl_handle, hid_data->reports[i].cfg_hdl, (uint8_t *)&data, sizeof(data));
                                hid_data->report_idx = i + 1;
                                break;
                            }
                        }
                        if (i >= HID_MAX_REPORT) {
                            atomic_set_bit(&device->flags, BT_DEV_HID_INTR_READY);
                            bt_hid_init(device);
                        }
                    }
                    break;
                }
            }
            break;
        }
        case BT_ATT_OP_READ_BLOB_RSP:
        {
            struct bt_att_read_blob_rsp *read_blob_rsp = (struct bt_att_read_blob_rsp *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_READ_BLOB_RSP\n");

            switch (device->hid_state) {
                case BT_ATT_HID_REPORT_MAP:
                    if (bt_data->base.sdp_data == NULL) {
                        bt_data->base.sdp_data = malloc(BT_SDP_DATA_SIZE);
                        if (bt_data->base.sdp_data == NULL) {
                            printf("# dev: %ld Failed to alloc report memory\n", device->ids.id);
                            break;
                        }
                    }
                    memcpy(bt_data->base.sdp_data + bt_data->base.sdp_len, read_blob_rsp->value, att_len - 1);
                    bt_data->base.sdp_len += att_len - 1;

                    if (att_len == device->mtu) {
                        bt_att_cmd_read_blob_req(device->acl_handle, hid_data->map_hdl, bt_data->base.sdp_len);
                    }
                    else {
                        hid_parser(bt_data, bt_data->base.sdp_data, bt_data->base.sdp_len);
                        if (bt_data->base.sdp_data) {
                            free(bt_data->base.sdp_data);
                            bt_data->base.sdp_data = NULL;
                        }
                        if (hid_data->reports[hid_data->report_idx].ref_hdl) {
                            device->hid_state = BT_ATT_HID_REPORT_REF;
                            bt_att_cmd_read_req(device->acl_handle, hid_data->reports[hid_data->report_idx].ref_hdl);
                        }
                        else {
                            printf("# dev: %ld No report found!!\n", device->ids.id);
                        }
                    }
                    break;
            }
            break;
        }
        case BT_ATT_OP_READ_GROUP_RSP:
        {
            struct bt_att_read_group_rsp *read_group_rsp = (struct bt_att_read_group_rsp *)bt_hci_acl_pkt->att_data;
            const uint32_t elem_cnt = (att_len - 2)/read_group_rsp->len;
            printf("# BT_ATT_OP_READ_GROUP_RSP\n");

            if (read_group_rsp->len == 6) {
                struct bt_att_group_data_uuid16 *elem = (struct bt_att_group_data_uuid16 *)read_group_rsp->data;
                for (uint32_t i = 0; i < elem_cnt; i++) {
                    if (elem[i].uuid == BT_UUID_HIDS) {
                        hid_data->start_hdl = elem[i].start_handle;
                        hid_data->end_hdl = elem[i].end_handle;
                    }
                }
            }
            struct bt_att_group_data *last_elem = (struct bt_att_group_data *)((uint8_t *)read_group_rsp->data + read_group_rsp->len * (elem_cnt - 1));
            bt_att_cmd_read_group_req_uuid16(device->acl_handle, last_elem->end_handle, BT_UUID_GATT_PRIMARY);
            break;
        }
        case BT_ATT_OP_WRITE_RSP:
        {
            printf("# BT_ATT_OP_WRITE_RSP\n");

            switch (device->hid_state) {
                case BT_ATT_HID_REPORT_CFG:
                {
                    uint32_t i = 0;
                    for (i = hid_data->report_idx; i < HID_MAX_REPORT; i++) {
                        if (bt_att_is_report_required(&hid_data->reports[i], bt_data)) {
                            uint16_t data = 0x0001;
                            bt_att_cmd_write_req(device->acl_handle, hid_data->reports[i].cfg_hdl, (uint8_t *)&data, sizeof(data));
                            hid_data->report_idx = i + 1;
                            break;
                        }
                    }
                    if (i >= HID_MAX_REPORT) {
                        atomic_set_bit(&device->flags, BT_DEV_HID_INTR_READY);
                        bt_hid_init(device);
                    }
                    break;
                }
            }
            break;
        }
        case BT_ATT_OP_NOTIFY:
        {
            struct bt_att_notify *notify = (struct bt_att_notify *)bt_hci_acl_pkt->att_data;

            for (uint32_t i = 0; i < HID_MAX_REPORT; i++) {
                if (notify->handle == hid_data->reports[i].report_hdl) {
                    bt_host_bridge(device, hid_data->reports[i].id, notify->value, att_len - sizeof(notify->handle));
                    break;
                }
            }
        }
    }
}
