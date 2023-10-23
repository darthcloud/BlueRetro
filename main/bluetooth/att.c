/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "host.h"
#include "hci.h"
#include "att.h"
#include "zephyr/uuid.h"
#include "zephyr/att.h"
#include "zephyr/gatt.h"

static uint16_t max_mtu = 23;

void bt_att_cmd(uint16_t handle, uint8_t code, uint16_t len) {
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

void bt_att_cmd_error_rsp(uint16_t handle, uint8_t req_opcode, uint16_t err_handle, uint8_t err) {
    struct bt_att_error_rsp *error_rsp = (struct bt_att_error_rsp *)bt_hci_pkt_tmp.att_data;

    error_rsp->request = req_opcode;
    error_rsp->handle = err_handle;
    error_rsp->error = err;

    bt_att_cmd(handle, BT_ATT_OP_ERROR_RSP, sizeof(*error_rsp));
}

void bt_att_cmd_mtu_req(uint16_t handle, uint16_t mtu) {
    struct bt_att_exchange_mtu_req *mtu_req = (struct bt_att_exchange_mtu_req *)bt_hci_pkt_tmp.att_data;

    mtu_req->mtu = mtu;

    bt_att_cmd(handle, BT_ATT_OP_MTU_REQ, sizeof(*mtu_req));
}

void bt_att_cmd_mtu_rsp(uint16_t handle, uint16_t mtu) {
    struct bt_att_exchange_mtu_rsp *mtu_rsp = (struct bt_att_exchange_mtu_rsp *)bt_hci_pkt_tmp.att_data;

    mtu_rsp->mtu = mtu;

    bt_att_cmd(handle, BT_ATT_OP_MTU_RSP, sizeof(*mtu_rsp));
}

void bt_att_cmd_find_info_req(uint16_t handle, uint16_t start, uint16_t end) {
    struct bt_att_find_info_req *find_info_req = (struct bt_att_find_info_req *)bt_hci_pkt_tmp.att_data;

    find_info_req->start_handle = start;
    find_info_req->end_handle = end;

    bt_att_cmd(handle, BT_ATT_OP_FIND_INFO_REQ, sizeof(*find_info_req));
}

void bt_att_cmd_read_type_req_uuid16(uint16_t handle, uint16_t start, uint16_t end, uint16_t uuid) {
    struct bt_att_read_type_req *read_type_req = (struct bt_att_read_type_req *)bt_hci_pkt_tmp.att_data;
    printf("# %s\n", __FUNCTION__);

    read_type_req->start_handle = start;
    read_type_req->end_handle = end;
    *(uint16_t *)read_type_req->uuid = uuid;

    bt_att_cmd(handle, BT_ATT_OP_READ_TYPE_REQ, sizeof(read_type_req->start_handle)
        + sizeof(read_type_req->start_handle) + sizeof(uuid));
}

void bt_att_cmd_read_req(uint16_t handle, uint16_t att_handle) {
    struct bt_att_read_req *read_req = (struct bt_att_read_req *)bt_hci_pkt_tmp.att_data;

    read_req->handle = att_handle;

    bt_att_cmd(handle, BT_ATT_OP_READ_REQ, sizeof(*read_req));
}

void bt_att_cmd_read_blob_req(uint16_t handle, uint16_t att_handle, uint16_t offset) {
    struct bt_att_read_blob_req *read_blob_req = (struct bt_att_read_blob_req *)bt_hci_pkt_tmp.att_data;
    printf("# %s\n", __FUNCTION__);

    read_blob_req->handle = att_handle;
    read_blob_req->offset = offset;

    bt_att_cmd(handle, BT_ATT_OP_READ_BLOB_REQ, sizeof(*read_blob_req));
}

void bt_att_cmd_read_group_req_uuid16(uint16_t handle, uint16_t start, uint16_t uuid) {
    struct bt_att_read_group_req *read_group_req = (struct bt_att_read_group_req *)bt_hci_pkt_tmp.att_data;

    read_group_req->start_handle = start;
    read_group_req->end_handle = 0xFFFF;
    *(uint16_t *)read_group_req->uuid = uuid;

    bt_att_cmd(handle, BT_ATT_OP_READ_GROUP_REQ, sizeof(read_group_req->start_handle)
        + sizeof(read_group_req->start_handle) + sizeof(uuid));
}

void bt_att_cmd_write_req(uint16_t handle, uint16_t att_handle, uint8_t *data, uint32_t len) {
    struct bt_att_write_req *write_req = (struct bt_att_write_req *)bt_hci_pkt_tmp.att_data;

    write_req->handle = att_handle;
    memcpy(write_req->value, data, len);

    bt_att_cmd(handle, BT_ATT_OP_WRITE_REQ, sizeof(write_req->handle) + len);
}

void bt_att_cmd_write_cmd(uint16_t handle, uint16_t att_handle, uint8_t *data, uint32_t len) {
    struct bt_att_write_cmd *write_cmd = (struct bt_att_write_cmd *)bt_hci_pkt_tmp.att_data;

    write_cmd->handle = att_handle;
    memcpy(write_cmd->value, data, len);

    bt_att_cmd(handle, BT_ATT_OP_WRITE_CMD, sizeof(write_cmd->handle) + len);
}

void bt_att_cmd_wr_rsp(uint16_t handle) {

    bt_att_cmd(handle, BT_ATT_OP_WRITE_RSP, 0);
}

void bt_att_cmd_prep_wr_rsp(uint16_t handle, uint8_t *data, uint32_t data_len) {

    memcpy(bt_hci_pkt_tmp.att_data, data, data_len);

    bt_att_cmd(handle, BT_ATT_OP_PREPARE_WRITE_RSP, data_len);
}

void bt_att_cmd_exec_wr_rsp(uint16_t handle) {

    bt_att_cmd(handle, BT_ATT_OP_EXEC_WRITE_RSP, 0);
}

void bt_att_set_le_max_mtu(uint16_t le_max_mtu) {
    max_mtu = le_max_mtu;
    printf("# %s %d\n", __FUNCTION__, max_mtu);
}

uint16_t bt_att_get_le_max_mtu(void) {
    return max_mtu;
}
