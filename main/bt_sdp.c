#include <stdio.h>
#include "bt_host.h"
#include "bt_sdp.h"

static void bt_sdp_cmd(uint16_t handle, uint16_t cid, uint8_t code, uint8_t tid, uint16_t len) {
    uint16_t packet_len = (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
        + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_sdp_hdr) + len);

    bt_hci_pkt_tmp.h4_hdr.type = BT_HCI_H4_TYPE_ACL;

    bt_hci_pkt_tmp.acl_hdr.handle = bt_acl_handle_pack(handle, 0x2);
    bt_hci_pkt_tmp.acl_hdr.len = packet_len - BT_HCI_H4_HDR_SIZE - BT_HCI_ACL_HDR_SIZE;

    bt_hci_pkt_tmp.l2cap_hdr.len = bt_hci_pkt_tmp.acl_hdr.len - sizeof(bt_hci_pkt_tmp.l2cap_hdr);
    bt_hci_pkt_tmp.l2cap_hdr.cid = cid;

    bt_hci_pkt_tmp.sdp_hdr.op_code = code;
    bt_hci_pkt_tmp.sdp_hdr.tid = tid;
    bt_hci_pkt_tmp.sdp_hdr.param_len = len;

    bt_host_txq_add((uint8_t *)&bt_hci_pkt_tmp, packet_len);
}

static void bt_sdp_cmd_att_rsp(uint16_t handle, uint16_t cid, uint8_t tid) {
    struct bt_sdp_att_rsp *att_rsp = (struct bt_sdp_att_rsp *)bt_hci_pkt_tmp.sig_data;
    uint8_t *sdp_data = bt_hci_pkt_tmp.sig_data + sizeof(struct bt_sdp_att_rsp);

    att_rsp->att_list_len = 0;
    *sdp_data = 0;
    bt_sdp_cmd(handle, cid, BT_SDP_SVC_SEARCH_ATTR_RSP, tid, sizeof(struct bt_sdp_att_rsp) + 1);
}

void bt_sdp_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->sdp_hdr.op_code) {
        case BT_SDP_SVC_SEARCH_ATTR_REQ:
        {
            bt_sdp_cmd_att_rsp(device->acl_handle, device->sdp_chan.dcid, bt_hci_acl_pkt->sdp_hdr.tid);
            break;
        }
    }
}
