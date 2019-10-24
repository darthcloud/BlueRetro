#include <stdio.h>
#include "zephyr/types.h"
#include "zephyr/sdp.h"
#include "util.h"
#include "bt_host.h"
#include "bt_sdp.h"

const uint8_t test_attr_req[] = {
    /* Service Search Pattern */
        /* Data Element */
            /* Type */ BT_SDP_SEQ8,
            /* Size */ 0x03,
            /* Data Value */
                /* Data Element */
                    /* Type */ BT_SDP_UUID16,
                    /* Data Value */
                        /* UUID */ 0x01, 0x00, /* L2CAP */
    /* Max Att Byte */ 0x08, 0x00, /* 2048 */
    /* Att ID List */
        /* Data Element */
            /* Type */ BT_SDP_SEQ8,
            /* Size */ 0x05,
            /* Data Value */
                /* Data Element */
                    /* Type */ BT_SDP_UINT32,
                    /* Data Value */
                        /* Att Range */ 0xff, 0xfe, /* to */ 0xff, 0xff,
    /* Continuation State */ 0x00,
};

static const uint8_t xb1_svc_search_attr_rsp[] = {
    0x35, 0x0a, 0x35, 0x08, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x12, 0x00
};

static const uint8_t xb1_svc_search_rsp[] = {
    0x00, 0x01, 0x00, 0x00
};

static const uint8_t xb1_svc_attr_rsp[] = {
    0x35, 0x95, 0x09, 0x00, 0x00, 0x0a, 0x00, 0x01,
    0x00, 0x00, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19,
    0x12, 0x00, 0x09, 0x00, 0x04, 0x35, 0x0d, 0x35,
    0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x01, 0x35,
    0x03, 0x19, 0x00, 0x01, 0x09, 0x00, 0x05, 0x35,
    0x03, 0x19, 0x10, 0x02, 0x09, 0x00, 0x06, 0x35,
    0x09, 0x09, 0x65, 0x6e, 0x09, 0x00, 0x6a, 0x09,
    0x01, 0x00, 0x09, 0x01, 0x00, 0x25, 0x18, 0x44,
    0x65, 0x76, 0x69, 0x63, 0x65, 0x20, 0x49, 0x44,
    0x20, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65,
    0x20, 0x52, 0x65, 0x63, 0x6f, 0x72, 0x64, 0x09,
    0x01, 0x01, 0x25, 0x18, 0x44, 0x65, 0x76, 0x69,
    0x63, 0x65, 0x20, 0x49, 0x44, 0x20, 0x53, 0x65,
    0x72, 0x76, 0x69, 0x63, 0x65, 0x20, 0x52, 0x65,
    0x63, 0x6f, 0x72, 0x64, 0x09, 0x02, 0x00, 0x09,
    0x01, 0x03, 0x09, 0x02, 0x01, 0x09, 0x00, 0x06,
    0x09, 0x02, 0x02, 0x09, 0x00, 0x01, 0x09, 0x02,
    0x03, 0x09, 0x0a, 0x00, 0x09, 0x02, 0x04, 0x28,
    0x01, 0x09, 0x02, 0x05, 0x09, 0x00, 0x01
};

static uint16_t tx_ident = 0;

static void bt_sdp_cmd(uint16_t handle, uint16_t cid, uint8_t code, uint16_t tid, uint16_t len) {
    uint16_t packet_len = (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
        + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_sdp_hdr) + len);

    bt_hci_pkt_tmp.h4_hdr.type = BT_HCI_H4_TYPE_ACL;

    bt_hci_pkt_tmp.acl_hdr.handle = bt_acl_handle_pack(handle, 0x2);
    bt_hci_pkt_tmp.acl_hdr.len = packet_len - BT_HCI_H4_HDR_SIZE - BT_HCI_ACL_HDR_SIZE;

    bt_hci_pkt_tmp.l2cap_hdr.len = bt_hci_pkt_tmp.acl_hdr.len - sizeof(bt_hci_pkt_tmp.l2cap_hdr);
    bt_hci_pkt_tmp.l2cap_hdr.cid = cid;

    bt_hci_pkt_tmp.sdp_hdr.op_code = code;
    bt_hci_pkt_tmp.sdp_hdr.tid = sys_cpu_to_be16(tid);
    bt_hci_pkt_tmp.sdp_hdr.param_len = sys_cpu_to_be16(len);

    bt_host_txq_add((uint8_t *)&bt_hci_pkt_tmp, packet_len);
}

static void bt_sdp_cmd_svc_search_rsp(uint16_t handle, uint16_t cid, uint16_t tid, const uint8_t *data, uint32_t len) {
    struct bt_sdp_svc_rsp *svc_rsp = (struct bt_sdp_svc_rsp *)bt_hci_pkt_tmp.sdp_data;
    uint8_t *sdp_data = bt_hci_pkt_tmp.sdp_data + sizeof(struct bt_sdp_svc_rsp);
    uint8_t *sdp_con_state = sdp_data + len;

    svc_rsp->total_recs = sys_cpu_to_be16(0x0001);
    svc_rsp->current_recs = sys_cpu_to_be16(0x0001);
    if (data) {
        memcpy(sdp_data, data, len);
    }
    *sdp_con_state = 0;

    bt_sdp_cmd(handle, cid, BT_SDP_SVC_SEARCH_RSP, tid, sizeof(struct bt_sdp_svc_rsp) + len + 1);
}

static void bt_sdp_cmd_svc_attr_rsp(uint16_t handle, uint16_t cid, uint16_t tid, const uint8_t *data, uint32_t len) {
    struct bt_sdp_att_rsp *att_rsp = (struct bt_sdp_att_rsp *)bt_hci_pkt_tmp.sdp_data;
    uint8_t *sdp_data = bt_hci_pkt_tmp.sdp_data + sizeof(struct bt_sdp_att_rsp);
    uint8_t *sdp_con_state = sdp_data + len;

    att_rsp->att_list_len = sys_cpu_to_be16(len);
    if (data) {
        memcpy(sdp_data, data, len);
    }
    *sdp_con_state = 0;

    bt_sdp_cmd(handle, cid, BT_SDP_SVC_ATTR_RSP, tid, sizeof(struct bt_sdp_att_rsp) + len + 1);
}

void bt_sdp_cmd_svc_search_attr_req(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;

    memcpy(bt_hci_pkt_tmp.sdp_data, test_attr_req, sizeof(test_attr_req));

    bt_sdp_cmd(device->acl_handle, device->sdp_tx_chan.dcid, BT_SDP_SVC_SEARCH_ATTR_REQ, tx_ident++, sizeof(test_attr_req));
}

static void bt_sdp_cmd_svc_search_attr_rsp(uint16_t handle, uint16_t cid, uint16_t tid, const uint8_t *data, uint32_t len) {
    struct bt_sdp_att_rsp *att_rsp = (struct bt_sdp_att_rsp *)bt_hci_pkt_tmp.sdp_data;
    uint8_t *sdp_data = bt_hci_pkt_tmp.sdp_data + sizeof(struct bt_sdp_att_rsp);
    uint8_t *sdp_con_state = sdp_data + len;

    att_rsp->att_list_len = sys_cpu_to_be16(len);
    if (data) {
        memcpy(sdp_data, data, len);
    }
    *sdp_con_state = 0;

    bt_sdp_cmd(handle, cid, BT_SDP_SVC_SEARCH_ATTR_RSP, tid, sizeof(struct bt_sdp_att_rsp) + len + 1);
}

void bt_sdp_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->sdp_hdr.op_code) {
        case BT_SDP_SVC_SEARCH_REQ:
            bt_sdp_cmd_svc_search_rsp(device->acl_handle, device->sdp_rx_chan.dcid,
                sys_be16_to_cpu(bt_hci_acl_pkt->sdp_hdr.tid), xb1_svc_search_rsp, sizeof(xb1_svc_search_rsp));
            break;
        case BT_SDP_SVC_ATTR_REQ:
            bt_sdp_cmd_svc_attr_rsp(device->acl_handle, device->sdp_rx_chan.dcid,
                sys_be16_to_cpu(bt_hci_acl_pkt->sdp_hdr.tid), xb1_svc_attr_rsp, sizeof(xb1_svc_attr_rsp));
            break;
        case BT_SDP_SVC_SEARCH_ATTR_REQ:
            if (device->type == XB1_S) {
                /* Need to test if empty answer work for xb1 */
                bt_sdp_cmd_svc_search_attr_rsp(device->acl_handle, device->sdp_rx_chan.dcid,
                    sys_be16_to_cpu(bt_hci_acl_pkt->sdp_hdr.tid), xb1_svc_search_attr_rsp, sizeof(xb1_svc_search_attr_rsp));
            }
            else {
                bt_sdp_cmd_svc_search_attr_rsp(device->acl_handle, device->sdp_rx_chan.dcid,
                    sys_be16_to_cpu(bt_hci_acl_pkt->sdp_hdr.tid), NULL, 0);
            }
            break;
    }
}
