/*
 * Copyright (c) 2019-2023, Jacques Gagnon
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
#ifdef CONFIG_BLUERETRO_ADAPTER_RUMBLE_TEST
#include "bluetooth/hidp/xbox.h"
#endif

enum {
    BT_ATT_HID_DEVICE_NAME = 0,
    BT_ATT_HID_APPEARANCE,
    BT_ATT_HID_FIND_HID_HDLS,
    BT_ATT_HID_IDENT_HID_HLDS,
    BT_ATT_HID_CHAR_PROP,
    BT_ATT_HID_REPORT_MAP,
    BT_ATT_HID_REPORT_REF,
    BT_ATT_HID_REPORT_CFG,
    BT_ATT_HID_PPCP_CFG,
    BT_ATT_HID_BATT_LVL,
    BT_ATT_HID_INIT,
    BT_ATT_HID_STATE_MAX,
};

struct bt_att_read_type_data {
    uint16_t handle;
    uint8_t char_prop;
    uint16_t char_value_handle;
    uint16_t uuid;
} __packed;

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
    uint8_t char_prop;
    uint16_t report_hdl;
    uint16_t cfg_hdl;
    uint16_t ref_hdl;
};

struct bt_att_hid {
    char device_name[32];
    uint16_t appearance;
    uint16_t dev_name_hdl;
    uint16_t start_hdl;
    uint16_t end_hdl;
    uint16_t map_hdl;
    uint8_t report_cnt;
    uint8_t report_found;
    uint8_t report_idx;
    struct bt_att_hid_report reports[HID_MAX_REPORT];
};

typedef void (*bit_att_hid_start_func_t)(struct bt_dev *device,
    struct bt_att_hid *hid_data);
typedef void (*bit_att_hid_process_func_t)(struct bt_dev *device,
    struct bt_att_hid *hid_data, uint32_t att_len, uint8_t *data, uint32_t data_len);

static struct bt_att_hid att_hid[BT_MAX_DEV] = {0};

static void bt_att_hid_start_next_state(struct bt_dev *device,
        struct bt_att_hid *hid_data);

static void bt_att_hid_start_device_name(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    bt_att_cmd_read_type_req_uuid16(device->acl_handle, 0x0001, 0xFFFF, BT_UUID_GAP_DEVICE_NAME);
}

static void bt_att_hid_process_device_name(struct bt_dev *device,
        struct bt_att_hid *hid_data, uint32_t att_len,
        uint8_t *data, uint32_t data_len) {
    char tmp[32] = {0};
    uint32_t name_len = data_len + strlen(hid_data->device_name);

    if (data) {
        if (name_len < sizeof(hid_data->device_name)) {
            memcpy(tmp, data, data_len);
            strcat(hid_data->device_name, tmp);
            if (att_len == device->mtu) {
                bt_att_cmd_read_blob_req(device->acl_handle,
                    hid_data->dev_name_hdl, strlen(hid_data->device_name));
                return;
            }
        }
        else {
            memcpy(tmp, data,
                sizeof(hid_data->device_name) - strlen(hid_data->device_name) - 1);
            strcat(hid_data->device_name, tmp);
        }
    }

    bt_hid_set_type_flags_from_name(device, hid_data->device_name);
    printf("# dev: %ld type: %ld %s\n", device->ids.id, device->ids.type, hid_data->device_name);

    bt_att_hid_start_next_state(device, hid_data);
}

static void bt_att_hid_start_appearance(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    bt_att_cmd_read_type_req_uuid16(device->acl_handle, 0x0001, 0xFFFF, BT_UUID_GAP_APPEARANCE);
}

static void bt_att_hid_process_appearance(struct bt_dev *device,
        struct bt_att_hid *hid_data, uint32_t att_len, uint8_t *data, uint32_t data_len) {
    if (data) {
        hid_data->appearance = *(uint16_t *)data;
    }
    printf("# dev: %ld appearance: %03X:%02X\n",
        device->ids.id, hid_data->appearance >> 6, hid_data->appearance & 0x3F);
    bt_att_hid_start_next_state(device, hid_data);
}

static void bt_att_hid_start_find_hid_hdls(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    bt_att_cmd_read_group_req_uuid16(device->acl_handle, 0x0001, BT_UUID_GATT_PRIMARY);
}

static void bt_att_hid_process_find_hid_hdls(struct bt_dev *device,
        struct bt_att_hid *hid_data, uint32_t att_len, uint8_t *data, uint32_t data_len) {
    if (data) {
        const uint32_t elem_cnt = (att_len - 2) / data_len;

        if (data_len == 6) {
            struct bt_att_group_data_uuid16 *elem = (struct bt_att_group_data_uuid16 *)data;
            for (uint32_t i = 0; i < elem_cnt; i++) {
                if (elem[i].uuid == BT_UUID_HIDS) {
                    hid_data->start_hdl = elem[i].start_handle;
                    hid_data->end_hdl = elem[i].end_handle;
                }
            }
        }
        struct bt_att_group_data *last_elem = (struct bt_att_group_data *)((uint8_t *)data + data_len * (elem_cnt - 1));
        bt_att_cmd_read_group_req_uuid16(device->acl_handle, last_elem->end_handle, BT_UUID_GATT_PRIMARY);
    }
    else {
        bt_att_hid_start_next_state(device, hid_data);
    }
}

static void bt_att_hid_start_ident_hid_hdls(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    bt_att_cmd_find_info_req(device->acl_handle, hid_data->start_hdl, hid_data->end_hdl);
}

static void bt_att_hid_process_ident_hid_hdls(struct bt_dev *device,
        struct bt_att_hid *hid_data, uint32_t att_len, uint8_t *info, uint32_t format) {
    if (info) {
        uint16_t last_handle;
        struct bt_att_info_16 *info16;
        struct bt_att_info_128 *info128;
        uint32_t elem_cnt;

        if (format == BT_ATT_INFO_16) {
            info16 = (struct bt_att_info_16 *)info;
            elem_cnt = (att_len - 2)/sizeof(*info16);

            last_handle = info16[elem_cnt - 1].handle;
        }
        else {
            info128 = (struct bt_att_info_128 *)info;
            elem_cnt = (att_len - 2)/sizeof(*info128);

            last_handle = info128[elem_cnt - 1].handle;
        }

        for (uint32_t i = 0; i < elem_cnt; i++) {
            uint16_t handle;
            uint16_t uuid;

            if (format == BT_ATT_INFO_16) {
                handle = info16[i].handle;
                uuid = info16[i].uuid;
            }
            else {
                handle = info128[i].handle;
                uuid = *(uint16_t *)&info128[i].uuid[12];
            }

            printf("# INFO HDL: %04X UUID: %04X REPORT_CNT: %d\n", handle, uuid, hid_data->report_cnt);

            switch (uuid) {
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
                    hid_data->reports[hid_data->report_cnt].report_hdl = handle;
                    hid_data->report_found = 1;
                    break;
                case BT_UUID_GATT_CCC:
                    hid_data->reports[hid_data->report_cnt].cfg_hdl = handle;
                    break;
                case BT_UUID_HIDS_REPORT_REF:
                    hid_data->reports[hid_data->report_cnt].ref_hdl = handle;
                    break;
                case BT_UUID_HIDS_REPORT_MAP:
                    hid_data->map_hdl = handle;
                    break;
                case BT_UUID_HIDS_INFO:
                case BT_UUID_HIDS_CTRL_POINT:
                    break;
            }
        }

find_info_rsp_end:
        if (last_handle < hid_data->end_hdl) {
            bt_att_cmd_find_info_req(device->acl_handle, last_handle + 1, hid_data->end_hdl);
        }
        else {
            device->hid_state = BT_ATT_HID_CHAR_PROP;
            bt_att_cmd_read_type_req_uuid16(device->acl_handle,
                hid_data->start_hdl, hid_data->end_hdl, BT_UUID_GATT_CHRC);
        }
    }
    else {
        bt_att_hid_start_next_state(device, hid_data);
    }
}

static void bt_att_hid_start_char_prop(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    bt_att_cmd_read_type_req_uuid16(device->acl_handle,
        hid_data->start_hdl, hid_data->end_hdl, BT_UUID_GATT_CHRC);
}

static void bt_att_hid_process_char_prop(struct bt_dev *device,
        struct bt_att_hid *hid_data, uint32_t att_len, uint8_t *data, uint32_t data_len) {
    if (data) {
        struct bt_att_read_type_data *read_type_data = (struct bt_att_read_type_data *)data;
        const uint32_t elem_cnt = (att_len - 2) / data_len;
        uint16_t last_handle = read_type_data[elem_cnt - 1].handle;

        for (uint32_t i = 0; i < elem_cnt; i++) {
            for (uint32_t j = 0; j < HID_MAX_REPORT; j++) {
                if (hid_data->reports[j].report_hdl == read_type_data[i].char_value_handle) {
                    printf("# CHAR_PROP Handl: %04X Prop: %02X\n", read_type_data[i].char_value_handle, read_type_data[i].char_prop);
                    hid_data->reports[j].char_prop = read_type_data[i].char_prop;
                }
            }
        }

        if (last_handle < hid_data->end_hdl) {
            bt_att_cmd_read_type_req_uuid16(device->acl_handle,
                last_handle + 1, hid_data->end_hdl, BT_UUID_GATT_CHRC);
            return;
        }
    }
    bt_att_hid_start_next_state(device, hid_data);
}

static void bt_att_hid_start_report_map(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    bt_att_cmd_read_req(device->acl_handle, hid_data->map_hdl);
}

static void bt_att_hid_process_report_map(struct bt_dev *device,
        struct bt_att_hid *hid_data, uint32_t att_len, uint8_t *data, uint32_t data_len) {
    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];

    if (bt_data->base.sdp_data == NULL) {
        bt_data->base.sdp_data = malloc(BT_SDP_DATA_SIZE);
        if (bt_data->base.sdp_data == NULL) {
            printf("# dev: %ld Failed to alloc report memory\n", device->ids.id);
            return;
        }
    }

    if (data) {
        memcpy(bt_data->base.sdp_data + bt_data->base.sdp_len, data, data_len);
        bt_data->base.sdp_len += data_len;

        if (att_len == device->mtu) {
            bt_att_cmd_read_blob_req(device->acl_handle, hid_data->map_hdl, bt_data->base.sdp_len);
            return;
        }
    }

    hid_parser(bt_data, bt_data->base.sdp_data, bt_data->base.sdp_len);
    if (bt_data->base.sdp_data) {
        free(bt_data->base.sdp_data);
        bt_data->base.sdp_data = NULL;
    }
    if (hid_data->reports[hid_data->report_idx].ref_hdl) {
        bt_att_hid_start_next_state(device, hid_data);
    }
    else {
        printf("# dev: %ld No report found!!\n", device->ids.id);
    }
}

static void bt_att_hid_start_report_ref(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    hid_data->report_idx = 0;
    bt_att_cmd_read_req(device->acl_handle, hid_data->reports[hid_data->report_idx].ref_hdl);
}

static void bt_att_hid_process_report_ref(struct bt_dev *device,
        struct bt_att_hid *hid_data, uint32_t att_len, uint8_t *data, uint32_t data_len) {
    if (data) {
        struct bt_att_report_ref *report_ref = (struct bt_att_report_ref *)data;

        hid_data->reports[hid_data->report_idx].id = report_ref->report_id;
        hid_data->reports[hid_data->report_idx++].type = report_ref->report_type;

        if (hid_data->report_idx < HID_MAX_REPORT && hid_data->reports[hid_data->report_idx].ref_hdl) {
            bt_att_cmd_read_req(device->acl_handle, hid_data->reports[hid_data->report_idx].ref_hdl);
            return;
        }
    }
    bt_att_hid_start_next_state(device, hid_data);
}

static void bt_att_hid_continue_report_cfg(struct bt_dev *device,
        struct bt_att_hid *hid_data, uint32_t att_len, uint8_t *data, uint32_t data_len) {
    uint32_t i = hid_data->report_idx;
    for (; i < HID_MAX_REPORT; i++) {
        if (hid_data->reports[i].char_prop & BT_GATT_CHRC_NOTIFY) {
            uint16_t data = BT_GATT_CCC_NOTIFY;
            bt_att_cmd_write_req(device->acl_handle, hid_data->reports[i].cfg_hdl, (uint8_t *)&data, sizeof(data));
            hid_data->report_idx = i + 1;
            break;
        }
    }
    if (i >= HID_MAX_REPORT) {
        bt_att_hid_start_next_state(device, hid_data);
    }
}

static void bt_att_hid_start_report_cfg(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    hid_data->report_idx = 0;
    bt_att_hid_continue_report_cfg(device, hid_data, 0, NULL, 0);
}

static void bt_att_hid_start_ppcp_cfg(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    bt_att_cmd_read_type_req_uuid16(device->acl_handle, 0x0001, 0xFFFF, BT_UUID_GAP_PPCP);
}

static void bt_att_hid_process_ppcp_cfg(struct bt_dev *device,
        struct bt_att_hid *hid_data, uint32_t att_len, uint8_t *data, uint32_t data_len) {
    if (data) {
        struct bt_l2cap_conn_param_req *conn_param_req = (struct bt_l2cap_conn_param_req *)data;
        struct hci_cp_le_conn_update le_conn_update = {0};

        le_conn_update.handle = device->acl_handle;
        le_conn_update.conn_interval_min = conn_param_req->min_interval;
        le_conn_update.conn_interval_max = conn_param_req->max_interval;
        le_conn_update.conn_latency = conn_param_req->latency;
        le_conn_update.supervision_timeout = conn_param_req->timeout;
        le_conn_update.min_ce_len = 0;
        le_conn_update.max_ce_len = 0;

        bt_hci_le_conn_update(&le_conn_update);
    }
    bt_att_hid_start_next_state(device, hid_data);
}

static void bt_att_hid_start_batt_lvl(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    bt_att_cmd_read_type_req_uuid16(device->acl_handle, 0x0001, 0xFFFF, BT_UUID_BAS_BATTERY_LEVEL);
}

static void bt_att_hid_process_batt_lvl(struct bt_dev *device,
        struct bt_att_hid *hid_data, uint32_t att_len, uint8_t *data, uint32_t data_len) {
    bt_att_hid_start_next_state(device, hid_data);
}

static void bt_att_hid_start_init(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    if (!atomic_test_bit(&device->flags, BT_DEV_HID_INTR_READY)) {
        atomic_set_bit(&device->flags, BT_DEV_HID_INTR_READY);
        bt_hid_init(device);
    }
}

static bit_att_hid_start_func_t start_state_func[BT_ATT_HID_STATE_MAX] = {
    bt_att_hid_start_device_name,
    bt_att_hid_start_appearance,
    bt_att_hid_start_find_hid_hdls,
    bt_att_hid_start_ident_hid_hdls,
    bt_att_hid_start_char_prop,
    bt_att_hid_start_report_map,
    bt_att_hid_start_report_ref,
    bt_att_hid_start_report_cfg,
    bt_att_hid_start_ppcp_cfg,
    bt_att_hid_start_batt_lvl,
    bt_att_hid_start_init, /* Need to be last one */
};
static bit_att_hid_process_func_t process_state_func[BT_ATT_HID_STATE_MAX] = {
    bt_att_hid_process_device_name,
    bt_att_hid_process_appearance,
    bt_att_hid_process_find_hid_hdls,
    bt_att_hid_process_ident_hid_hdls,
    bt_att_hid_process_char_prop,
    bt_att_hid_process_report_map,
    bt_att_hid_process_report_ref,
    bt_att_hid_continue_report_cfg,
    bt_att_hid_process_ppcp_cfg,
    bt_att_hid_process_batt_lvl,
    NULL, /* Need to be last one */
};

static void bt_att_hid_start_first_state(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    device->hid_state = 0;
    if (device->hid_state < BT_ATT_HID_STATE_MAX && start_state_func[0]) {
        start_state_func[0](device, hid_data);
    }
}

static void bt_att_hid_process_current_state(struct bt_dev *device,
        struct bt_att_hid *hid_data, uint32_t att_len, uint8_t *data, uint32_t data_len) {
    if (device->hid_state < BT_ATT_HID_STATE_MAX && process_state_func[device->hid_state]) {
        process_state_func[device->hid_state](device, hid_data, att_len, data, data_len);
    }
}

static void bt_att_hid_start_next_state(struct bt_dev *device,
        struct bt_att_hid *hid_data) {
    device->hid_state++;
    if (device->hid_state < BT_ATT_HID_STATE_MAX && start_state_func[device->hid_state]) {
        start_state_func[device->hid_state](device, hid_data);
    }
}

static uint32_t bt_att_get_report_index(struct bt_att_hid *hid_data, uint8_t report_id) {
    for (uint32_t i = 0; i < HID_MAX_REPORT; i++) {
        if (hid_data->reports[i].id == report_id) {
            return i;
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
    struct bt_att_hid *hid_data = &att_hid[device->ids.id];
    uint32_t index = bt_att_get_report_index(hid_data, report_id);
    uint16_t att_handle = hid_data->reports[index].report_hdl;
    uint8_t char_prop = hid_data->reports[index].char_prop;

    if (att_handle) {
        if (char_prop & BT_GATT_CHRC_WRITE) {
            bt_att_cmd_write_req(device->acl_handle, att_handle, data, len);
        }
        else {
            bt_att_cmd_write_cmd(device->acl_handle, att_handle, data, len);
        }
    }
}

void bt_att_hid_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    struct bt_att_hid *hid_data = &att_hid[device->ids.id];
    uint32_t att_len = len - (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE + sizeof(struct bt_l2cap_hdr));

    switch (bt_hci_acl_pkt->att_hdr.code) {
        case BT_ATT_OP_ERROR_RSP:
            if (!atomic_test_bit(&device->flags, BT_DEV_HID_INTR_READY)) {
                bt_att_hid_process_current_state(device, hid_data, att_len, NULL, 0);
            }
            break;
        case BT_ATT_OP_MTU_RSP:
        {
            struct bt_att_exchange_mtu_rsp *mtu_rsp = (struct bt_att_exchange_mtu_rsp *)bt_hci_acl_pkt->att_data;

            device->mtu = mtu_rsp->mtu;

            if (!atomic_test_bit(&device->flags, BT_DEV_HID_INTR_READY)) {
                bt_hci_stop_inquiry();
                bt_att_hid_start_first_state(device, hid_data);
            }
            break;
        }
        case BT_ATT_OP_FIND_INFO_RSP:
        {
            struct bt_att_find_info_rsp *find_info_rsp = (struct bt_att_find_info_rsp *)bt_hci_acl_pkt->att_data;

            if (!atomic_test_bit(&device->flags, BT_DEV_HID_INTR_READY)) {
                switch (device->hid_state) {
                    case BT_ATT_HID_IDENT_HID_HLDS:
                        bt_att_hid_process_ident_hid_hdls(device, hid_data, att_len, find_info_rsp->info, find_info_rsp->format);
                        break;
                    default:
                        printf("# %s: Invalid state: %ld rsp: 0x%02X\n", __FUNCTION__, device->hid_state, bt_hci_acl_pkt->att_hdr.code);
                        break;
                }
            }
            break;
        }
        case BT_ATT_OP_READ_TYPE_RSP:
        {
            struct bt_att_read_type_rsp *read_type_rsp = (struct bt_att_read_type_rsp *)bt_hci_acl_pkt->att_data;
            uint8_t rsp_len = read_type_rsp->len - sizeof(read_type_rsp->data[0].handle);

            if (!atomic_test_bit(&device->flags, BT_DEV_HID_INTR_READY)) {
                switch (device->hid_state) {
                    case BT_ATT_HID_DEVICE_NAME:
                        hid_data->dev_name_hdl = read_type_rsp->data[0].handle;
                        bt_att_hid_process_device_name(device, hid_data, att_len, read_type_rsp->data[0].value, rsp_len);
                        break;
                    case BT_ATT_HID_APPEARANCE:
                        bt_att_hid_process_appearance(device, hid_data, att_len, read_type_rsp->data[0].value, rsp_len);
                        break;
                    case BT_ATT_HID_CHAR_PROP:
                        bt_att_hid_process_char_prop(device, hid_data, att_len, (uint8_t *)read_type_rsp->data, read_type_rsp->len);
                        break;
                    case BT_ATT_HID_PPCP_CFG:
                        bt_att_hid_process_ppcp_cfg(device, hid_data, att_len, read_type_rsp->data[0].value, rsp_len);
                        break;
                    case BT_ATT_HID_BATT_LVL:
                        bt_att_hid_process_batt_lvl(device, hid_data, att_len, read_type_rsp->data[0].value, rsp_len);
                        break;
                    default:
                        printf("# %s: Invalid state: %ld rsp: 0x%02X\n", __FUNCTION__, device->hid_state, bt_hci_acl_pkt->att_hdr.code);
                        break;
                }
            }
            break;
        }
        case BT_ATT_OP_READ_RSP:
        {
            struct bt_att_read_rsp *read_rsp = (struct bt_att_read_rsp *)bt_hci_acl_pkt->att_data;

            if (!atomic_test_bit(&device->flags, BT_DEV_HID_INTR_READY)) {
                switch (device->hid_state) {
                    case BT_ATT_HID_REPORT_MAP:
                        bt_att_hid_process_report_map(device, hid_data, att_len, read_rsp->value, att_len - 1);
                        break;
                    case BT_ATT_HID_REPORT_REF:
                        bt_att_hid_process_report_ref(device, hid_data, att_len, read_rsp->value, att_len - 1);
                        break;
                    default:
                        printf("# %s: Invalid state: %ld rsp: 0x%02X\n", __FUNCTION__, device->hid_state, bt_hci_acl_pkt->att_hdr.code);
                        break;
                }
            }
            break;
        }
        case BT_ATT_OP_READ_BLOB_RSP:
        {
            struct bt_att_read_blob_rsp *read_blob_rsp = (struct bt_att_read_blob_rsp *)bt_hci_acl_pkt->att_data;

            if (!atomic_test_bit(&device->flags, BT_DEV_HID_INTR_READY)) {
                switch (device->hid_state) {
                    case BT_ATT_HID_DEVICE_NAME:
                        bt_att_hid_process_device_name(device, hid_data, att_len, read_blob_rsp->value, att_len - 1);
                        break;
                    case BT_ATT_HID_REPORT_MAP:
                        bt_att_hid_process_report_map(device, hid_data, att_len, read_blob_rsp->value, att_len - 1);
                        break;
                    default:
                        printf("# %s: Invalid state: %ld rsp: 0x%02X\n", __FUNCTION__, device->hid_state, bt_hci_acl_pkt->att_hdr.code);
                        break;
                }
            }
            break;
        }
        case BT_ATT_OP_READ_GROUP_RSP:
        {
            struct bt_att_read_group_rsp *read_group_rsp = (struct bt_att_read_group_rsp *)bt_hci_acl_pkt->att_data;

            if (!atomic_test_bit(&device->flags, BT_DEV_HID_INTR_READY)) {
                switch (device->hid_state) {
                    case BT_ATT_HID_FIND_HID_HDLS:
                        bt_att_hid_process_find_hid_hdls(device, hid_data, att_len, (uint8_t *)read_group_rsp->data, read_group_rsp->len);
                        break;
                    default:
                        printf("# %s: Invalid state: %ld rsp: 0x%02X\n", __FUNCTION__, device->hid_state, bt_hci_acl_pkt->att_hdr.code);
                        break;
                }
            }
            break;
        }
        case BT_ATT_OP_WRITE_RSP:
        {
            if (!atomic_test_bit(&device->flags, BT_DEV_HID_INTR_READY)) {
                switch (device->hid_state) {
                    case BT_ATT_HID_REPORT_CFG:
                        bt_att_hid_continue_report_cfg(device, hid_data, att_len, NULL, 0);
                        break;
                    default:
                        printf("# %s: Invalid state: %ld rsp: 0x%02X\n", __FUNCTION__, device->hid_state, bt_hci_acl_pkt->att_hdr.code);
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
#ifdef CONFIG_BLUERETRO_ADAPTER_RUMBLE_TEST
                    struct bt_hidp_xb1_rumble rumble = {
                        .enable = 0x03,
                        .duration = 0xFF,
                        .cnt = 0x00,
                    };
                    rumble.mag_r = bt_hci_acl_pkt->hidp_data[11];
                    rumble.mag_l = bt_hci_acl_pkt->hidp_data[9];
                    bt_hid_cmd_xbox_rumble(device, &rumble);
#else
                    bt_host_bridge(device, hid_data->reports[i].id, notify->value, att_len - sizeof(notify->handle));
#endif
                    break;
                }
            }
        }
    }
}
