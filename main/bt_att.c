#include <stdio.h>
#include "bt_host.h"
#include "bt_att.h"
#include "zephyr/uuid.h"
#include "zephyr/att.h"
#include "zephyr/gatt.h"
#include "config.h"

#define ATT_MAX_LEN 512

enum {
    GATT_GRP_HDL = 0x0001,
    GATT_SRVC_CH_ATT_HDL,
    GATT_SRVC_CH_CHRC_HDL,
    GAP_GRP_HDL = 0x0014,
    GAP_DEV_NAME_ATT_HDL,
    GAP_DEV_NAME_CHRC_HDL,
    GAP_APP_ATT_HDL,
    GAP_APP_CHRC_HDL,
    GAP_CAR_ATT_HDL,
    GAP_CAR_CHRC_HDL,
    BATT_GRP_HDL = 0x0028,
    BATT_ATT_HDL,
    BATT_CHRC_HDL,
    BATT_CHRC_CONF_HDL,
    BATT_CHRC_DESC_HDL,
    BR_GRP_HDL = 0x0040,
    BR_GLBL_CFG_ATT_HDL,
    BR_GLBL_CFG_CHRC_HDL,
    BR_OUT_CFG_CTRL_ATT_HDL,
    BR_OUT_CFG_CTRL_CHRC_HDL,
    BR_OUT_CFG_DATA_ATT_HDL,
    BR_OUT_CFG_DATA_CHRC_HDL,
    BR_IN_CFG_CTRL_ATT_HDL,
    BR_IN_CFG_CTRL_CHRC_HDL,
    BR_IN_CFG_DATA_ATT_HDL,
    BR_IN_CFG_DATA_CHRC_HDL,
    MAX_HDL,
};

static uint8_t br_grp_base_uuid[] = {0x00, 0x9a, 0x79, 0x76, 0xa1, 0x2f, 0x4b, 0x31, 0xb0, 0xfa, 0x80, 0x51, 0x56, 0x0f, 0x83, 0x56};

static uint16_t max_mtu = 23;
static uint16_t mtu = 23;
static uint8_t power = 0;
static uint16_t out_ctrl_cfg_id = 0;
static uint16_t ctrl_offset = 0;
static uint16_t ctrl_cfg_id = 0;

static void bt_att_cmd(uint16_t handle, uint8_t code, uint16_t len) {
    uint16_t packet_len = (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
        + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_att_hdr) + len);

    bt_hci_pkt_tmp.h4_hdr.type = BT_HCI_H4_TYPE_ACL;

    bt_hci_pkt_tmp.acl_hdr.handle = bt_acl_handle_pack(handle, 0x2);
    bt_hci_pkt_tmp.acl_hdr.len = packet_len - BT_HCI_H4_HDR_SIZE - BT_HCI_ACL_HDR_SIZE;

    bt_hci_pkt_tmp.l2cap_hdr.len = bt_hci_pkt_tmp.acl_hdr.len - sizeof(bt_hci_pkt_tmp.l2cap_hdr);
    bt_hci_pkt_tmp.l2cap_hdr.cid = BT_L2CAP_CID_ATT;

    bt_hci_pkt_tmp.att_hdr.code = code;

    bt_host_txq_add((uint8_t *)&bt_hci_pkt_tmp, packet_len);
}

static void bt_att_cmd_error_rsp(uint16_t handle, uint8_t req_opcode, uint16_t err_handle, uint8_t err) {
    struct bt_att_error_rsp *error_rsp = (struct bt_att_error_rsp *)bt_hci_pkt_tmp.att_data;
    printf("# %s\n", __FUNCTION__);

    error_rsp->request = req_opcode;
    error_rsp->handle = err_handle;
    error_rsp->error = err;

    bt_att_cmd(handle, BT_ATT_OP_ERROR_RSP, sizeof(*error_rsp));
}

static void bt_att_cmd_mtu_rsp(uint16_t handle, uint16_t mtu) {
    struct bt_att_exchange_mtu_rsp *mtu_rsp = (struct bt_att_exchange_mtu_rsp *)bt_hci_pkt_tmp.att_data;
    printf("# %s\n", __FUNCTION__);

    mtu_rsp->mtu = mtu;

    bt_att_cmd(handle, BT_ATT_OP_MTU_RSP, sizeof(*mtu_rsp));
}

static void bt_att_cmd_find_info_rsp_uuid16(uint16_t handle, uint16_t start) {
    struct bt_att_find_info_rsp *find_info_rsp = (struct bt_att_find_info_rsp *)bt_hci_pkt_tmp.att_data;
    struct bt_att_info_16 *info = (struct bt_att_info_16 *)find_info_rsp->info;
    printf("# %s\n", __FUNCTION__);

    find_info_rsp->format = BT_ATT_INFO_16;
    info->handle = start;

    switch (start) {
        case BATT_CHRC_CONF_HDL:
            info->uuid = BT_UUID_GATT_CCC;
            break;
        case BATT_CHRC_DESC_HDL:
            info->uuid = BT_UUID_GATT_CUD;
            break;
    }

    bt_att_cmd(handle, BT_ATT_OP_FIND_INFO_RSP, 5);
}

static void bt_att_cmd_find_info_rsp_uuid128(uint16_t handle, uint16_t start) {
    printf("# %s\n", __FUNCTION__);
}

static void bt_att_cmd_gatt_char_read_type_rsp(uint16_t handle) {
    struct bt_att_read_type_rsp *rd_type_rsp = (struct bt_att_read_type_rsp *)bt_hci_pkt_tmp.att_data;
    uint8_t *data = rd_type_rsp->data->value;

    printf("# %s\n", __FUNCTION__);

    rd_type_rsp->len = 7;

    rd_type_rsp->data->handle = GATT_SRVC_CH_ATT_HDL;
    *data = BT_GATT_CHRC_INDICATE;
    data++;
    *(uint16_t *)data = GATT_SRVC_CH_CHRC_HDL;
    data += 2;
    *(uint16_t *)data = BT_UUID_GATT_SC;

    bt_att_cmd(handle, BT_ATT_OP_READ_TYPE_RSP, sizeof(rd_type_rsp->len) + rd_type_rsp->len);
}

static void bt_att_cmd_gap_char_read_type_rsp(uint16_t handle) {
    struct bt_att_read_type_rsp *rd_type_rsp = (struct bt_att_read_type_rsp *)bt_hci_pkt_tmp.att_data;
    struct bt_att_data *name_data = (struct bt_att_data *)((uint8_t *)rd_type_rsp->data + 0);
    struct bt_att_data *app_data = (struct bt_att_data *)((uint8_t *)rd_type_rsp->data + 7);
    struct bt_att_data *car_data = (struct bt_att_data *)((uint8_t *)rd_type_rsp->data + 14);
    uint8_t *data;

    printf("# %s\n", __FUNCTION__);

    rd_type_rsp->len = 7;

    name_data->handle = GAP_DEV_NAME_ATT_HDL;
    data = name_data->value;
    *data = BT_GATT_CHRC_READ;
    data++;
    *(uint16_t *)data = GAP_DEV_NAME_CHRC_HDL;
    data += 2;
    *(uint16_t *)data = BT_UUID_GAP_DEVICE_NAME;

    app_data->handle = GAP_APP_ATT_HDL;
    data = app_data->value;
    *data = BT_GATT_CHRC_READ;
    data++;
    *(uint16_t *)data = GAP_APP_CHRC_HDL;
    data += 2;
    *(uint16_t *)data = BT_UUID_GAP_APPEARANCE;

    car_data->handle = GAP_CAR_ATT_HDL;
    data = car_data->value;
    *data = BT_GATT_CHRC_READ;
    data++;
    *(uint16_t *)data = GAP_CAR_CHRC_HDL;
    data += 2;
    *(uint16_t *)data = BT_UUID_CENTRAL_ADDR_RES;

    bt_att_cmd(handle, BT_ATT_OP_READ_TYPE_RSP, sizeof(rd_type_rsp->len) + rd_type_rsp->len*3);
}

static void bt_att_cmd_batt_char_read_type_rsp(uint16_t handle) {
    struct bt_att_read_type_rsp *rd_type_rsp = (struct bt_att_read_type_rsp *)bt_hci_pkt_tmp.att_data;
    uint8_t *data = rd_type_rsp->data->value;

    printf("# %s\n", __FUNCTION__);

    rd_type_rsp->len = 7;

    rd_type_rsp->data->handle = BATT_ATT_HDL;
    *data = BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_READ;
    data++;
    *(uint16_t *)data = BATT_CHRC_HDL;
    data += 2;
    *(uint16_t *)data = BT_UUID_BAS_BATTERY_LEVEL;

    bt_att_cmd(handle, BT_ATT_OP_READ_TYPE_RSP, sizeof(rd_type_rsp->len) + rd_type_rsp->len);
}

static void bt_att_cmd_blueretro_char_read_type_rsp(uint16_t handle, uint16_t start) {
    struct bt_att_read_type_rsp *rd_type_rsp = (struct bt_att_read_type_rsp *)bt_hci_pkt_tmp.att_data;
    uint8_t *data = rd_type_rsp->data->value;

    printf("# %s\n", __FUNCTION__);

    rd_type_rsp->len = 21;

    if (!(start & 0x01)) {
        rd_type_rsp->data->handle = start + 1;
    }
    else {
        rd_type_rsp->data->handle = start;
    }
    *data = BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE;
    data++;
    *(uint16_t *)data = rd_type_rsp->data->handle + 1;
    data += 2;
    memcpy(data, br_grp_base_uuid, sizeof(br_grp_base_uuid));
    *data = (rd_type_rsp->data->handle - BR_GLBL_CFG_ATT_HDL)/2 + 1;

    bt_att_cmd(handle, BT_ATT_OP_READ_TYPE_RSP, sizeof(rd_type_rsp->len) + rd_type_rsp->len);
}

static void bt_att_cmd_dev_name_rd_rsp(uint16_t handle) {
    char *str = "BlueRetro";
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.att_data, str, strlen(str));

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, strlen(str));
}

static void bt_att_cmd_app_rd_rsp(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);

    *(uint16_t *)bt_hci_pkt_tmp.att_data = 964; /* HID Gamepad */

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, sizeof(uint16_t));
}

static void bt_att_cmd_car_rd_rsp(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);

    *bt_hci_pkt_tmp.att_data = 0x00;

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, sizeof(uint8_t));
}

static void bt_att_cmd_batt_lvl_rd_rsp(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);

    *bt_hci_pkt_tmp.att_data = power++;

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, sizeof(uint8_t));
}

static void bt_att_cmd_config_rd_rsp(uint16_t handle, uint8_t config_id, uint16_t offset) {
    uint32_t len = 0;
    printf("# %s\n", __FUNCTION__);

    if (config_id == 0) {
        printf("# Global config\n");
        len = sizeof(config.global_cfg);
        memcpy(bt_hci_pkt_tmp.att_data, (void *)&config.global_cfg, len);
    }
    else if (config_id == 2) {
        printf("# Output config\n");
        if (offset > sizeof(config.out_cfg[0])) {
            len = 0;
        }
        else {
            len = sizeof(config.out_cfg[0]) - offset;

            if (len > (mtu - 1)) {
                len = mtu - 1;
            }

            memcpy(bt_hci_pkt_tmp.att_data, (void *)&config.out_cfg[out_ctrl_cfg_id] + offset, len);
        }
    }
    else if (config_id == 4) {
        uint32_t cfg_len = (sizeof(config.in_cfg[0]) - (ADAPTER_MAPPING_MAX * sizeof(config.in_cfg[0].map_cfg[0]) - config.in_cfg[ctrl_cfg_id].map_size * sizeof(config.in_cfg[0].map_cfg[0])));
        uint32_t sum_offset = ctrl_offset + offset;
        printf("# Input config %d\n", cfg_len);
        if (sum_offset > cfg_len || offset > ATT_MAX_LEN) {
            len = 0;
        }
        else {
            len = cfg_len - sum_offset;

            if (len > (mtu - 1)) {
                len = mtu - 1;
            }

            if ((offset + len) > ATT_MAX_LEN) {
                len = ATT_MAX_LEN - offset;
            }

            memcpy(bt_hci_pkt_tmp.att_data, (void *)&config.in_cfg[ctrl_cfg_id] + sum_offset, len);
        }
    }

    bt_att_cmd(handle, offset ? BT_ATT_OP_READ_BLOB_RSP : BT_ATT_OP_READ_RSP, len);
}

static void bt_att_cmd_conf_rd_rsp(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);

    *(uint16_t *)bt_hci_pkt_tmp.att_data = 0x0000;

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, sizeof(uint16_t));
}

static void bt_att_cmd_read_group_rsp(uint16_t handle, uint16_t start, uint16_t end) {
    struct bt_att_read_group_rsp *rd_grp_rsp = (struct bt_att_read_group_rsp *)bt_hci_pkt_tmp.att_data;
    struct bt_att_group_data *gatt_data = (struct bt_att_group_data *)((uint8_t *)rd_grp_rsp->data + 0);
    struct bt_att_group_data *gap_data = (struct bt_att_group_data *)((uint8_t *)rd_grp_rsp->data + 6);
    struct bt_att_group_data *batt_data = (struct bt_att_group_data *)((uint8_t *)rd_grp_rsp->data + 12);
    uint32_t len = sizeof(*rd_grp_rsp) - sizeof(rd_grp_rsp->data);

    printf("# %s\n", __FUNCTION__);

    if (start <= BATT_CHRC_DESC_HDL) {
        rd_grp_rsp->len = 6;

        if (start <= GATT_GRP_HDL && end >= GATT_SRVC_CH_CHRC_HDL) {
            gatt_data->start_handle = GATT_GRP_HDL;
            gatt_data->end_handle = GATT_SRVC_CH_CHRC_HDL;
            *(uint16_t *)gatt_data->value = BT_UUID_GATT;
            len += rd_grp_rsp->len;
        }

        if (start <= GAP_GRP_HDL && end >= GAP_CAR_CHRC_HDL) {
            gap_data->start_handle = GAP_GRP_HDL;
            gap_data->end_handle = GAP_CAR_CHRC_HDL;
            *(uint16_t *)gap_data->value = BT_UUID_GAP;
            len += rd_grp_rsp->len;
        }

        if (start <= BATT_GRP_HDL && end >= BATT_CHRC_DESC_HDL) {
            batt_data->start_handle = BATT_GRP_HDL;
            batt_data->end_handle = BATT_CHRC_DESC_HDL;
            *(uint16_t *)batt_data->value = BT_UUID_BAS;
            len += rd_grp_rsp->len;
        }
    }
    else {
        rd_grp_rsp->len = 20;

        if (start <= BR_GRP_HDL && end >= BR_IN_CFG_DATA_CHRC_HDL) {
            gatt_data->start_handle = BR_GRP_HDL;
            gatt_data->end_handle = BR_IN_CFG_DATA_CHRC_HDL;
            memcpy(gatt_data->value, br_grp_base_uuid, sizeof(br_grp_base_uuid));
            len += rd_grp_rsp->len;
        }
    }

    bt_att_cmd(handle, BT_ATT_OP_READ_GROUP_RSP, len);
}

static void bt_att_cmd_wr_rsp(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);

    bt_att_cmd(handle, BT_ATT_OP_WRITE_RSP, 0);
}

void bt_att_set_le_max_mtu(uint16_t le_max_mtu) {
    max_mtu = le_max_mtu;
}

void bt_att_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    switch (bt_hci_acl_pkt->att_hdr.code) {
        case BT_ATT_OP_MTU_REQ:
        {
            struct bt_att_exchange_mtu_req *mtu_req = (struct bt_att_exchange_mtu_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_MTU_REQ\n");
            if (mtu_req->mtu < max_mtu) {
                mtu = mtu_req->mtu;
            }
            else {
                mtu = max_mtu;
            }
            bt_att_cmd_mtu_rsp(device->acl_handle, mtu);
            break;
        }
        case BT_ATT_OP_FIND_INFO_REQ:
        {
            struct bt_att_find_info_req *find_info_req = (struct bt_att_find_info_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_FIND_INFO_REQ\n");
            if (find_info_req->start_handle > BATT_CHRC_HDL && find_info_req->start_handle < MAX_HDL) {
                bt_att_cmd_find_info_rsp_uuid16(device->acl_handle, find_info_req->start_handle);
            }
            else {
                bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_FIND_INFO_REQ, find_info_req->start_handle, BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
            }
            break;
        }
        case BT_ATT_OP_READ_TYPE_REQ:
        {
            struct bt_att_read_type_req *rd_type_req = (struct bt_att_read_type_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_READ_TYPE_REQ\n");
            uint16_t start = rd_type_req->start_handle;
            uint16_t end = rd_type_req->end_handle;

            if (*(uint16_t *)rd_type_req->uuid == BT_UUID_GATT_CHRC) {
                /* GATT */
                if (start >= GATT_GRP_HDL && start < GATT_SRVC_CH_CHRC_HDL && end >= GATT_SRVC_CH_CHRC_HDL) {
                    bt_att_cmd_gatt_char_read_type_rsp(device->acl_handle);
                }
                /* GAP */
                else if (start >= GATT_SRVC_CH_CHRC_HDL && start < GAP_CAR_CHRC_HDL && end >= GAP_CAR_CHRC_HDL) {
                    bt_att_cmd_gap_char_read_type_rsp(device->acl_handle);
                }
                /* BATT */
                else if (start >= GAP_CAR_CHRC_HDL && start < BATT_CHRC_HDL && end >= BATT_CHRC_HDL) {
                    bt_att_cmd_batt_char_read_type_rsp(device->acl_handle);
                }
                /* BLUERETRO */
                else if (start >= BATT_CHRC_HDL && start < BR_IN_CFG_DATA_CHRC_HDL && end >= BR_IN_CFG_DATA_CHRC_HDL) {
                    bt_att_cmd_blueretro_char_read_type_rsp(device->acl_handle, start);
                }
                else {
                    bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_READ_TYPE_REQ, rd_type_req->start_handle, BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
                }
            }
            else {
                bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_READ_TYPE_REQ, rd_type_req->start_handle, BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
            }
            break;
        }
        case BT_ATT_OP_READ_REQ:
        {
            struct bt_att_read_req *rd_req = (struct bt_att_read_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_READ_REQ\n");

            switch (rd_req->handle) {
                case GAP_DEV_NAME_CHRC_HDL:
                case BATT_CHRC_DESC_HDL:
                    bt_att_cmd_dev_name_rd_rsp(device->acl_handle);
                    break;
                case GAP_APP_CHRC_HDL:
                    bt_att_cmd_app_rd_rsp(device->acl_handle);
                    break;
                case GAP_CAR_CHRC_HDL:
                    bt_att_cmd_car_rd_rsp(device->acl_handle);
                    break;
                case BATT_CHRC_HDL:
                    bt_att_cmd_batt_lvl_rd_rsp(device->acl_handle);
                    break;
                case BATT_CHRC_CONF_HDL:
                    bt_att_cmd_conf_rd_rsp(device->acl_handle);
                    break;
                case BR_GLBL_CFG_CHRC_HDL:
                case BR_OUT_CFG_CTRL_CHRC_HDL:
                case BR_OUT_CFG_DATA_CHRC_HDL:
                case BR_IN_CFG_CTRL_CHRC_HDL:
                case BR_IN_CFG_DATA_CHRC_HDL:
                    bt_att_cmd_config_rd_rsp(device->acl_handle, (rd_req->handle - BR_GLBL_CFG_CHRC_HDL) / 2, 0);
                    break;
                default:
                    bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_READ_REQ, rd_req->handle, BT_ATT_ERR_INVALID_HANDLE);
                    break;
            }
            break;
        }
        case BT_ATT_OP_READ_BLOB_REQ:
        {
            struct bt_att_read_blob_req *rd_blob_req = (struct bt_att_read_blob_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_READ_BLOB_RSP\n");

            switch (rd_blob_req->handle) {
                case BR_GLBL_CFG_CHRC_HDL:
                case BR_OUT_CFG_CTRL_CHRC_HDL:
                case BR_OUT_CFG_DATA_CHRC_HDL:
                case BR_IN_CFG_CTRL_CHRC_HDL:
                case BR_IN_CFG_DATA_CHRC_HDL:
                    bt_att_cmd_config_rd_rsp(device->acl_handle, (rd_blob_req->handle - BR_GLBL_CFG_CHRC_HDL)/2, rd_blob_req->offset);
                    break;
                default:
                    bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_READ_BLOB_REQ, rd_blob_req->handle, BT_ATT_ERR_INVALID_HANDLE);
                    break;
            }
            break;
        }
        case BT_ATT_OP_READ_GROUP_REQ:
        {
            struct bt_att_read_group_req *rd_grp_req = (struct bt_att_read_group_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_READ_GROUP_REQ\n");
            if (rd_grp_req->start_handle < MAX_HDL && *(uint16_t *)rd_grp_req->uuid == BT_UUID_GATT_PRIMARY) {
                bt_att_cmd_read_group_rsp(device->acl_handle, rd_grp_req->start_handle, rd_grp_req->end_handle);
            }
            else {
                bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_READ_GROUP_REQ, rd_grp_req->start_handle, BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
            }
            break;
        }
        case BT_ATT_OP_WRITE_REQ:
        {
            struct bt_att_write_req *wr_req = (struct bt_att_write_req *)bt_hci_acl_pkt->att_data;
            uint16_t *data = (uint16_t *)wr_req->value;
            printf("# BT_ATT_OP_WRITE_REQ\n");
            switch (wr_req->handle) {
                case BR_GLBL_CFG_CHRC_HDL:
                    printf("# BR_GLBL_CFG_CHRC_HDL %04X\n", wr_req->handle);
                    memcpy((void *)&config.global_cfg, wr_req->value, sizeof(config.global_cfg));
                    config_update();
                    break;
                case BR_OUT_CFG_DATA_CHRC_HDL:
                    memcpy((void *)&config.out_cfg[out_ctrl_cfg_id], wr_req->value, sizeof(config.out_cfg[0]));
                    config_update();
                    break;
                case BR_IN_CFG_DATA_CHRC_HDL:
                    break;
                case BR_OUT_CFG_CTRL_CHRC_HDL:
                    out_ctrl_cfg_id = *data;
                    break;
                case BR_IN_CFG_CTRL_CHRC_HDL:
                    ctrl_cfg_id = *data;
                    data++;
                    ctrl_offset = *data;
                    break;
                default:
                    bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_WRITE_REQ, wr_req->handle, BT_ATT_ERR_INVALID_HANDLE);
                    break;
            }
            bt_att_cmd_wr_rsp(device->acl_handle);
            break;
        }
    }
}
