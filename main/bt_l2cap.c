#include <stdio.h>
#include "bt_host.h"
#include "bt_l2cap.h"

static void bt_l2cap_cmd(uint16_t handle, uint8_t code, uint8_t ident, uint16_t len) {
    uint16_t packet_len = (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
        + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_l2cap_sig_hdr) + len);

    bt_hci_pkt_tmp.h4_hdr.type = BT_HCI_H4_TYPE_ACL;

    bt_hci_pkt_tmp.acl_hdr.handle = bt_acl_handle_pack(handle, 0x2);
    bt_hci_pkt_tmp.acl_hdr.len = packet_len - BT_HCI_H4_HDR_SIZE - BT_HCI_ACL_HDR_SIZE;

    bt_hci_pkt_tmp.l2cap_hdr.len = bt_hci_pkt_tmp.acl_hdr.len - sizeof(bt_hci_pkt_tmp.l2cap_hdr);
    bt_hci_pkt_tmp.l2cap_hdr.cid = BT_L2CAP_CID_BR_SIG;

    bt_hci_pkt_tmp.sig_hdr.code = code;
    bt_hci_pkt_tmp.sig_hdr.ident = ident;
    bt_hci_pkt_tmp.sig_hdr.len = len;

    bt_host_txq_add((uint8_t *)&bt_hci_pkt_tmp, packet_len);
}

static void bt_l2cap_cmd_conn_req(uint16_t handle, uint8_t ident, uint16_t psm, uint16_t scid) {
    struct bt_l2cap_conn_req *conn_req = (struct bt_l2cap_conn_req *)bt_hci_pkt_tmp.sig_data;

    conn_req->psm = psm;
    conn_req->scid = scid;

    bt_l2cap_cmd(handle, BT_L2CAP_CONN_REQ, ident, sizeof(*conn_req));
}

void bt_l2cap_cmd_sdp_conn_req(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_conn_req(device->acl_handle, device->l2cap_ident, BT_L2CAP_PSM_SDP, device->sdp_chan.scid);
}

void bt_l2cap_cmd_hid_ctrl_conn_req(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_conn_req(device->acl_handle, device->l2cap_ident, BT_L2CAP_PSM_HID_CTRL, device->ctrl_chan.scid);
}

void bt_l2cap_cmd_hid_intr_conn_req(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_conn_req(device->acl_handle, device->l2cap_ident, BT_L2CAP_PSM_HID_INTR, device->intr_chan.scid);
}

static void bt_l2cap_cmd_conn_rsp(uint16_t handle, uint8_t ident, uint16_t dcid, uint16_t scid, uint16_t result) {
    struct bt_l2cap_conn_rsp *conn_rsp = (struct bt_l2cap_conn_rsp *)bt_hci_pkt_tmp.sig_data;

    conn_rsp->dcid = dcid;
    conn_rsp->scid = scid;
    conn_rsp->result = result;
    conn_rsp->status = BT_L2CAP_CS_NO_INFO;

    bt_l2cap_cmd(handle, BT_L2CAP_CONN_RSP, ident, sizeof(*conn_rsp));
}

void bt_l2cap_cmd_sdp_conn_rsp(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_conn_rsp(device->acl_handle, device->l2cap_ident, device->sdp_chan.scid, device->sdp_chan.dcid, BT_L2CAP_BR_SUCCESS);
}

void bt_l2cap_cmd_hid_ctrl_conn_rsp(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_conn_rsp(device->acl_handle, device->l2cap_ident, device->ctrl_chan.scid, device->ctrl_chan.dcid, BT_L2CAP_BR_SUCCESS);
}

void bt_l2cap_cmd_hid_intr_conn_rsp(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_conn_rsp(device->acl_handle, device->l2cap_ident, device->intr_chan.scid, device->intr_chan.dcid, BT_L2CAP_BR_SUCCESS);
}

static void bt_l2cap_cmd_conf_req(uint16_t handle, uint8_t ident, uint16_t dcid) {
    struct bt_l2cap_conf_req *conf_req = (struct bt_l2cap_conf_req *)bt_hci_pkt_tmp.sig_data;
    struct bt_l2cap_conf_opt *conf_opt = (struct bt_l2cap_conf_opt *)conf_req->data;

    conf_req->dcid = dcid;
    conf_req->flags = 0x0000;
    conf_opt->type = BT_L2CAP_CONF_OPT_MTU;
    conf_opt->len = sizeof(uint16_t);
    *(uint16_t *)conf_opt->data = 0xFFFF;

    bt_l2cap_cmd(handle, BT_L2CAP_CONF_REQ, ident,
        ((sizeof(*conf_req) - sizeof(conf_req->data)) + (sizeof(*conf_opt) - sizeof(conf_opt->data)) + sizeof(uint16_t)));
}

void bt_l2cap_cmd_sdp_conf_req(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_conf_req(device->acl_handle, device->l2cap_ident, device->sdp_chan.dcid);
}

void bt_l2cap_cmd_hid_ctrl_conf_req(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_conf_req(device->acl_handle, device->l2cap_ident, device->ctrl_chan.dcid);
}

void bt_l2cap_cmd_hid_intr_conf_req(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_conf_req(device->acl_handle, device->l2cap_ident, device->intr_chan.dcid);
}

static void bt_l2cap_cmd_conf_rsp(uint16_t handle, uint8_t ident, uint16_t scid) {
    struct bt_l2cap_conf_rsp *conf_rsp = (struct bt_l2cap_conf_rsp *)bt_hci_pkt_tmp.sig_data;
    struct bt_l2cap_conf_opt *conf_opt = (struct bt_l2cap_conf_opt *)conf_rsp->data;

    conf_rsp->scid = scid;
    conf_rsp->flags = 0x0000;
    conf_rsp->result = BT_L2CAP_CONF_SUCCESS;
    conf_opt->type = BT_L2CAP_CONF_OPT_MTU;
    conf_opt->len = sizeof(uint16_t);
    *(uint16_t *)conf_opt->data = 0x02A0;

    bt_l2cap_cmd(handle, BT_L2CAP_CONF_RSP, ident,
        ((sizeof(*conf_rsp) - sizeof(conf_rsp->data)) + (sizeof(*conf_opt) - sizeof(conf_opt->data)) + sizeof(uint16_t)));
}

void bt_l2cap_cmd_sdp_conf_rsp(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_conf_rsp(device->acl_handle, device->l2cap_ident, device->sdp_chan.dcid);
}

void bt_l2cap_cmd_hid_ctrl_conf_rsp(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_conf_rsp(device->acl_handle, device->l2cap_ident, device->ctrl_chan.dcid);
}

void bt_l2cap_cmd_hid_intr_conf_rsp(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_conf_rsp(device->acl_handle, device->l2cap_ident, device->intr_chan.dcid);
}

static void bt_l2cap_cmd_disconn_req(uint16_t handle, uint8_t ident, uint16_t dcid, uint16_t scid) {
    struct bt_l2cap_disconn_req *disconn_req = (struct bt_l2cap_disconn_req *)bt_hci_pkt_tmp.sig_data;

    disconn_req->dcid = dcid;
    disconn_req->scid = scid;

    bt_l2cap_cmd(handle, BT_L2CAP_DISCONN_REQ, ident, sizeof(*disconn_req));
}

void bt_l2cap_cmd_sdp_disconn_req(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_disconn_req(device->acl_handle, device->l2cap_ident, device->sdp_chan.dcid, device->sdp_chan.scid);
}

void bt_l2cap_cmd_hid_ctrl_disconn_req(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_disconn_req(device->acl_handle, device->l2cap_ident, device->ctrl_chan.dcid, device->ctrl_chan.scid);
}

void bt_l2cap_cmd_hid_intr_disconn_req(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_disconn_req(device->acl_handle, device->l2cap_ident, device->intr_chan.dcid, device->intr_chan.scid);
}

static void bt_l2cap_cmd_disconn_rsp(uint16_t handle, uint8_t ident, uint16_t dcid, uint16_t scid) {
    struct bt_l2cap_disconn_rsp *disconn_rsp = (struct bt_l2cap_disconn_rsp *)bt_hci_pkt_tmp.sig_data;

    disconn_rsp->dcid = dcid;
    disconn_rsp->scid = scid;

    bt_l2cap_cmd(handle, BT_L2CAP_DISCONN_RSP, ident, sizeof(*disconn_rsp));
}

void bt_l2cap_cmd_sdp_disconn_rsp(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_disconn_rsp(device->acl_handle, device->l2cap_ident, device->sdp_chan.scid, device->sdp_chan.dcid);
}

void bt_l2cap_cmd_hid_ctrl_disconn_rsp(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_disconn_rsp(device->acl_handle, device->l2cap_ident, device->ctrl_chan.scid, device->ctrl_chan.dcid);
}

void bt_l2cap_cmd_hid_intr_disconn_rsp(void *bt_dev) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    printf("# %s\n", __FUNCTION__);
    bt_l2cap_cmd_disconn_rsp(device->acl_handle, device->l2cap_ident, device->intr_chan.scid, device->intr_chan.dcid);
}
