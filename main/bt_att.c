#include <stdio.h>
#include "bt_host.h"
#include "bt_att.h"
#include "zephyr/att.h"

enum {
    GATT_GRP_HDL = 0x0001,
    GATT_ATT_SRVC_CH_HDL,
    GATT_CHAR_SRVC_CH_HDL,
    GAP_GRP_HDL = 0x0014,
    GAP_ATT_DEV_NAME_HDL,
    GAP_CHAR_DEV_NAME_HDL,
    GAP_ATT_APP_HDL,
    GAP_CHAR_APP_HDL,
    GAP_ATT_CAR_HDL,
    GAP_CHAR_CAR_HDL,
    BATT_GRP_HDL = 0x0028,
    BATT_ATT_HDL,
    BATT_CHAR_HDL,
    BATT_CHAR_CONF_HDL,
    BATT_CHAR_DESC_HDL,
    MAX_HDL,
};

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

static void bt_att_cmd_mtu_rsp(uint16_t handle, uint16_t mtu) {
    struct bt_att_exchange_mtu_rsp *mtu_rsp = (struct bt_att_exchange_mtu_rsp *)bt_hci_pkt_tmp.att_data;
    printf("# %s\n", __FUNCTION__);

    mtu_rsp->mtu = mtu;

    bt_att_cmd(handle, BT_ATT_OP_MTU_RSP, sizeof(*mtu_rsp));
}

static void bt_att_cmd_error_rsp(uint16_t handle, uint8_t req_opcode, uint16_t err_handle, uint8_t err) {
    printf("# %s\n", __FUNCTION__);
}

static void bt_att_cmd_find_info_rsp_uuid16(uint16_t handle, uint16_t start) {
    printf("# %s\n", __FUNCTION__);
}

static void bt_att_cmd_find_info_rsp_uuid128(uint16_t handle, uint16_t start) {
    printf("# %s\n", __FUNCTION__);
}

static void bt_att_cmd_gatt_read_type_rsp(uint16_t handle, uint16_t start, uint16_t end) {
    printf("# %s\n", __FUNCTION__);
}

static void bt_att_cmd_gap_read_type_rsp(uint16_t handle, uint16_t start, uint16_t end) {
    printf("# %s\n", __FUNCTION__);
}

static void bt_att_cmd_batt_read_type_rsp(uint16_t handle, uint16_t start, uint16_t end) {
    printf("# %s\n", __FUNCTION__);
}

static void bt_att_cmd_dev_name_rd_rsp(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);
}

static void bt_att_cmd_app_rd_rsp(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);
}

static void bt_att_cmd_car_rd_rsp(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);
}

static void bt_att_cmd_batt_lvl_rd_rsp(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);
}

static void bt_att_cmd_conf_rd_rsp(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);
}

static void bt_att_cmd_read_group_rsp(uint16_t handle, uint16_t start, uint16_t end) {
    printf("# %s\n", __FUNCTION__);
}

void bt_att_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    switch (bt_hci_acl_pkt->att_hdr.code) {
        case BT_ATT_OP_MTU_REQ:
        {
            struct bt_att_exchange_mtu_req *mtu_req = (struct bt_att_exchange_mtu_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_MTU_REQ\n");
            bt_att_cmd_mtu_rsp(device->acl_handle, mtu_req->mtu);
            break;
        }
        case BT_ATT_OP_FIND_INFO_REQ:
        {
            struct bt_att_find_info_req *find_info_req = (struct bt_att_find_info_req *)bt_hci_acl_pkt->att_data;
            printf("BT_ATT_OP_FIND_INFO_REQ\n");
            if (find_info_req->start_handle > BATT_CHAR_HDL && find_info_req->start_handle < MAX_HDL) {
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
            printf("BT_ATT_OP_READ_TYPE_REQ\n");

            /* GATT */
            if (rd_type_req->start_handle > GATT_GRP_HDL && rd_type_req->start_handle < GATT_CHAR_SRVC_CH_HDL) {
                bt_att_cmd_gatt_read_type_rsp(device->acl_handle, rd_type_req->start_handle, rd_type_req->end_handle);
            }
            /* GAP */
            else if (rd_type_req->start_handle > GAP_GRP_HDL && rd_type_req->start_handle < GAP_CHAR_CAR_HDL) {
                bt_att_cmd_gap_read_type_rsp(device->acl_handle, rd_type_req->start_handle, rd_type_req->end_handle);
            }
            /* BATT */
            else if (rd_type_req->start_handle > BATT_GRP_HDL && rd_type_req->start_handle < BATT_CHAR_HDL) {
                bt_att_cmd_batt_read_type_rsp(device->acl_handle, rd_type_req->start_handle, rd_type_req->end_handle);
            }
            else {
                bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_READ_TYPE_REQ, rd_type_req->start_handle, BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
            }
            break;
        }
        case BT_ATT_OP_READ_REQ:
        {
            struct bt_att_read_req *rd_req = (struct bt_att_read_req *)bt_hci_acl_pkt->att_data;
            printf("BT_ATT_OP_READ_REQ\n");

            switch (rd_req->handle) {
                case GAP_CHAR_DEV_NAME_HDL:
                    bt_att_cmd_dev_name_rd_rsp(device->acl_handle);
                    break;
                case GAP_CHAR_APP_HDL:
                    bt_att_cmd_app_rd_rsp(device->acl_handle);
                    break;
                case GAP_CHAR_CAR_HDL:
                    bt_att_cmd_car_rd_rsp(device->acl_handle);
                    break;
                case BATT_CHAR_HDL:
                    bt_att_cmd_batt_lvl_rd_rsp(device->acl_handle);
                    break;
                case BATT_CHAR_CONF_HDL:
                    bt_att_cmd_conf_rd_rsp(device->acl_handle);
                    break;
                case BATT_CHAR_DESC_HDL:
                    bt_att_cmd_dev_name_rd_rsp(device->acl_handle);
                    break;
                default:
                    bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_READ_REQ, rd_req->handle, BT_ATT_ERR_INVALID_HANDLE);
            }
            break;
        }
        case BT_ATT_OP_READ_GROUP_REQ:
        {
            struct bt_att_read_group_req *rd_grp_req = (struct bt_att_read_group_req *)bt_hci_acl_pkt->att_data;
            printf("BT_ATT_OP_READ_GROUP_REQ\n");
            bt_att_cmd_read_group_rsp(device->acl_handle, rd_grp_req->start_handle, rd_grp_req->end_handle);
            break;
        }
        case BT_ATT_OP_WRITE_REQ:
        {
            struct bt_att_write_req *wr_req = (struct bt_att_write_req *)bt_hci_acl_pkt->att_data;
            printf("BT_ATT_OP_WRITE_REQ\n");
            break;
        }
    }
}
