#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include <esp_bt.h>
#include <nvs_flash.h>
#include "zephyr/hci.h"
#include "zephyr/l2cap_internal.h"
#include "zephyr/atomic.h"
#include "hidp.h"
#include "adapter.h"

#if 0
static void bt_l2cap_cmd(uint16_t handle, uint16_t cid, uint8_t code, uint8_t ident, uint16_t len) {
    uint16_t buflen = (sizeof(bt_acl_frame.h4_type) + sizeof(bt_acl_frame.acl_hdr)
                       + sizeof(bt_acl_frame.l2cap_hdr) + sizeof(bt_acl_frame.pl.sig_hdr) + len);

    atomic_clear_bit(&bt_flags, BT_CTRL_READY);

    bt_acl_frame.h4_type = H4_TYPE_ACL;

    bt_acl_frame.acl_hdr.handle = handle | (0x2 << 12); /* BC/PB Flags (2 bits each) */
    bt_acl_frame.acl_hdr.len = buflen - sizeof(bt_acl_frame.h4_type) - sizeof(bt_acl_frame.acl_hdr);

    bt_acl_frame.l2cap_hdr.len = bt_acl_frame.acl_hdr.len - sizeof(bt_acl_frame.l2cap_hdr);
    bt_acl_frame.l2cap_hdr.cid = cid;

    bt_acl_frame.pl.sig_hdr.code = code;
    bt_acl_frame.pl.sig_hdr.ident = ident;
    bt_acl_frame.pl.sig_hdr.len = len;

#ifdef H4_TRACE
    bt_h4_trace((uint8_t *)&bt_acl_frame, buflen, BT_TX);
#endif /* H4_TRACE */

    esp_vhci_host_send_packet((uint8_t *)&bt_acl_frame, buflen);
}

static void bt_l2cap_cmd_conn_req(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t psm, uint16_t scid) {
    printf("# %s\n", __FUNCTION__);

    bt_acl_frame.pl.l2cap_data.conn_req.psm = psm;
    bt_acl_frame.pl.l2cap_data.conn_req.scid = scid;

    atomic_set_bit(&bt_flags, BT_CTRL_PENDING);
    bt_l2cap_cmd(handle, cid, BT_L2CAP_CONN_REQ, ident, sizeof(bt_acl_frame.pl.l2cap_data.conn_req));
}

static void bt_l2cap_cmd_conn_rsp(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t dcid, uint16_t scid, uint16_t result) {
    printf("# %s\n", __FUNCTION__);

    bt_acl_frame.pl.l2cap_data.conn_rsp.dcid = dcid;
    bt_acl_frame.pl.l2cap_data.conn_rsp.scid = scid;
    bt_acl_frame.pl.l2cap_data.conn_rsp.result = result;
    bt_acl_frame.pl.l2cap_data.conn_rsp.status = 0x0000;

    bt_l2cap_cmd(handle, cid, BT_L2CAP_CONN_RSP, ident, sizeof(bt_acl_frame.pl.l2cap_data.conn_rsp));
}

static void bt_l2cap_cmd_conf_req(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t dcid) {
    struct bt_l2cap_conf_opt *conf_opt = (struct bt_l2cap_conf_opt *)bt_acl_frame.pl.l2cap_data.conf_req.data;
    printf("# %s\n", __FUNCTION__);

    bt_acl_frame.pl.l2cap_data.conf_req.dcid = dcid;
    bt_acl_frame.pl.l2cap_data.conf_req.flags = 0x0000;
    conf_opt->type = BT_L2CAP_CONF_OPT_MTU;
    conf_opt->len = sizeof(uint16_t);
    *(uint16_t *)conf_opt->data = 0xFFFF;

    atomic_set_bit(&bt_flags, BT_CTRL_PENDING);
    bt_l2cap_cmd(handle, cid, BT_L2CAP_CONF_REQ, ident, sizeof(bt_acl_frame.pl.l2cap_data.conf_req));
}

static void bt_l2cap_cmd_conf_rsp(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t scid) {
    struct bt_l2cap_conf_opt *conf_opt = (struct bt_l2cap_conf_opt *)bt_acl_frame.pl.l2cap_data.conf_rsp.data;
    printf("# %s\n", __FUNCTION__);

    bt_acl_frame.pl.l2cap_data.conf_rsp.scid = scid;
    bt_acl_frame.pl.l2cap_data.conf_rsp.flags = 0x0000;
    bt_acl_frame.pl.l2cap_data.conf_rsp.result = 0x0000;
    conf_opt->type = BT_L2CAP_CONF_OPT_MTU;
    conf_opt->len = sizeof(uint16_t);
    *(uint16_t *)conf_opt->data = 0x02A0;

    bt_l2cap_cmd(handle, cid, BT_L2CAP_CONF_RSP, ident, sizeof(bt_acl_frame.pl.l2cap_data.conf_rsp));
}

static void bt_l2cap_cmd_disconn_req(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t dcid, uint16_t scid) {
    printf("# %s\n", __FUNCTION__);

    bt_acl_frame.pl.l2cap_data.disconn_req.dcid = dcid;
    bt_acl_frame.pl.l2cap_data.disconn_req.scid = scid;

    atomic_set_bit(&bt_flags, BT_CTRL_PENDING);
    bt_l2cap_cmd(handle, cid, BT_L2CAP_DISCONN_REQ, ident, sizeof(bt_acl_frame.pl.l2cap_data.disconn_req));
}

static void bt_l2cap_cmd_disconn_rsp(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t dcid, uint16_t scid) {
    printf("# %s\n", __FUNCTION__);

    bt_acl_frame.pl.l2cap_data.disconn_rsp.dcid = dcid;
    bt_acl_frame.pl.l2cap_data.disconn_rsp.scid = scid;

    bt_l2cap_cmd(handle, cid, BT_L2CAP_DISCONN_RSP, ident, sizeof(bt_acl_frame.pl.l2cap_data.disconn_rsp));
}

static void bt_l2cap_cmd_info_rsp(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t type) {
    printf("# %s\n", __FUNCTION__);

    bt_acl_frame.pl.l2cap_data.info_rsp.type = type;
    bt_acl_frame.pl.l2cap_data.info_rsp.result = 0x0000;
    memset(bt_acl_frame.pl.l2cap_data.info_rsp.data, 0x00, sizeof(bt_acl_frame.pl.l2cap_data.info_rsp.data));

    bt_l2cap_cmd(handle, cid, BT_L2CAP_INFO_RSP, ident, sizeof(bt_acl_frame.pl.l2cap_data.info_rsp));
}
#endif
