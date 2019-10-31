#include <stdio.h>
#include "bt_host.h"
#include "bt_att.h"

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

void bt_att_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->att_hdr.code) {
        case BT_ATT_OP_MTU_REQ:
        {
            struct bt_att_exchange_mtu_req *mtu_req = (struct bt_att_exchange_mtu_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_MTU_REQ\n");
            bt_att_cmd_mtu_rsp(device->acl_handle, mtu_req->mtu);
            break;
        }
    }
}
