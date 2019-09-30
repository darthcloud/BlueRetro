#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_bt.h>
#include <nvs_flash.h>
#include "zephyr/hci.h"
#include "zephyr/l2cap_internal.h"
#include "zephyr/atomic.h"
#include "bt.h"
#include "hidp.h"
#include "adapter.h"

#define H4_TRACE /* Display packet dump that can be parsed by wireshark/text2pcap */

#define BT_TX 0
#define BT_RX 1

#ifndef min
#define min(A,B) ((A) <= (B) ? (A) : (B))
#endif

#ifndef max
#define max(A,B) ((A) < (B) ? (B) : (A))
#endif

struct bt_hci_tx_frame {
    uint8_t h4_type;
    struct bt_hci_cmd_hdr cmd_hdr;
    union {
        struct bt_hci_cp_inquiry inquiry;
        struct bt_hci_cp_connect connect;
        struct bt_hci_cp_disconnect disconnect;
        struct bt_hci_cp_connect_cancel connect_cancel;
        struct bt_hci_cp_accept_conn_req accept_conn_req;
        struct bt_hci_cp_setup_sync_conn setup_sync_conn;
        struct bt_hci_cp_accept_sync_conn_req accept_sync_conn_req;
        struct bt_hci_cp_reject_conn_req reject_conn_req;
        struct bt_hci_cp_link_key_reply link_key_reply;
        struct bt_hci_cp_link_key_neg_reply link_key_neg_reply;
        struct bt_hci_cp_pin_code_reply pin_code_reply;
        struct bt_hci_cp_pin_code_neg_reply pin_code_neg_reply;
        struct bt_hci_cp_auth_requested auth_requested;
        struct bt_hci_cp_set_conn_encrypt set_conn_encrypt;
        struct bt_hci_cp_remote_name_request remote_name_request;
        struct bt_hci_cp_remote_name_cancel remote_name_cancel;
        struct bt_hci_cp_read_remote_features read_remote_features;
        struct bt_hci_cp_read_remote_ext_features read_remote_ext_features;
        struct bt_hci_cp_read_remote_version_info read_remote_version_info;
        struct bt_hci_cp_io_capability_reply io_capability_reply;
        struct bt_hci_cp_user_confirm_reply user_confirm_reply;
        struct bt_hci_cp_user_passkey_reply user_passkey_reply;
        struct bt_hci_cp_user_passkey_neg_reply user_passkey_neg_reply;
        struct bt_hci_cp_io_capability_neg_reply io_capability_neg_reply;
        struct bt_hci_cp_switch_role switch_role;
        struct bt_hci_cp_read_link_policy read_link_policy;
        struct bt_hci_cp_write_link_policy write_link_policy;
        struct bt_hci_cp_write_default_link_policy write_default_link_policy;
        struct bt_hci_cp_set_event_mask set_event_mask;
        struct bt_hci_cp_set_event_filter set_event_filter;
        struct bt_hci_cp_write_local_name write_local_name;
        struct bt_hci_cp_write_scan_enable write_scan_enable;
        struct bt_hci_cp_write_class_of_device write_class_of_device;
        struct bt_hci_cp_read_tx_power_level read_tx_power_level;
        struct bt_hci_cp_set_ctl_to_host_flow set_ctl_to_host_flow;
        struct bt_hci_cp_host_buffer_size host_buffer_size;
        struct bt_hci_cp_host_num_completed_packets host_num_completed_packets;
        struct bt_hci_cp_write_inquiry_mode write_inquiry_mode;
        struct bt_hci_cp_write_ssp_mode write_ssp_mode;
        struct bt_hci_cp_set_event_mask_page_2 set_event_mask_page_2;
        struct bt_hci_cp_write_le_host_supp write_le_host_supp;
        struct bt_hci_cp_write_sc_host_supp write_sc_host_supp;
        struct bt_hci_cp_read_auth_payload_timeout read_auth_payload_timeout;
        struct bt_hci_cp_write_auth_payload_timeout write_auth_payload_timeout;
        struct bt_hci_cp_read_local_ext_features read_local_ext_features;
        struct bt_hci_cp_read_rssi read_rssi;
        struct bt_hci_cp_read_encryption_key_size read_encryption_key_size;
    } cmd_cp;
} __packed;

struct bt_hci_rx_frame {
    uint8_t h4_type;
    struct bt_hci_evt_hdr evt_hdr;
    union {
        struct bt_hci_evt_inquiry_complete inquiry_complete;
        struct bt_hci_evt_inquiry_result inquiry_result;
        struct bt_hci_evt_conn_complete conn_complete;
        struct bt_hci_evt_conn_request conn_request;
        struct bt_hci_evt_disconn_complete disconn_complete;
        struct bt_hci_evt_auth_complete auth_complete;
        struct bt_hci_evt_remote_name_req_complete remote_name_req_complete;
        struct bt_hci_evt_encrypt_change encrypt_change;
        struct bt_hci_evt_remote_features remote_features;
        struct bt_hci_evt_remote_version_info remote_version_info;
        struct {
            struct bt_hci_evt_cmd_complete complete_hdr;
            union {
                uint8_t status;
                struct bt_hci_rp_connect_cancel connect_cancel;
                struct bt_hci_rp_link_key_neg_reply link_key_neg_reply;
                struct bt_hci_rp_pin_code_reply pin_code_reply;
                struct bt_hci_rp_pin_code_neg_reply pin_code_neg_reply;
                struct bt_hci_rp_remote_name_cancel remote_name_cancel;
                struct bt_hci_rp_user_confirm_reply user_confirm_reply;
                struct bt_hci_rp_read_link_policy read_link_policy;
                struct bt_hci_rp_write_link_policy write_link_policy;
                struct bt_hci_rp_read_default_link_policy read_default_link_policy;
                struct bt_hci_rp_read_tx_power_level read_tx_power_level;
                struct bt_hci_rp_read_auth_payload_timeout read_auth_payload_timeout;
                struct bt_hci_rp_write_auth_payload_timeout write_auth_payload_timeout;
                struct bt_hci_rp_read_local_version_info read_local_version_info;
                struct bt_hci_rp_read_supported_commands read_supported_commands;
                struct bt_hci_rp_read_local_ext_features read_local_ext_features;
                struct bt_hci_rp_read_local_features read_local_features;
                struct bt_hci_rp_read_buffer_size read_buffer_size;
                struct bt_hci_rp_read_bd_addr read_bd_addr;
                struct bt_hci_rp_read_rssi read_rssi;
                struct bt_hci_rp_read_encryption_key_size read_encryption_key_size;
            } complete_data;
        };
        struct bt_hci_evt_cc_status cc_status;
        struct bt_hci_evt_cmd_status cmd_status;
        struct bt_hci_evt_role_change role_change;
        struct bt_hci_evt_num_completed_packets num_completed_packets;
        struct bt_hci_evt_pin_code_req pin_code_req;
        struct bt_hci_evt_link_key_req link_key_req;
        struct bt_hci_evt_link_key_notify link_key_notify;
        struct bt_hci_evt_data_buf_overflow data_buf_overflow;
        struct bt_hci_evt_inquiry_result_with_rssi inquiry_result_with_rssi;
        struct bt_hci_evt_remote_ext_features remote_ext_features;
        struct bt_hci_evt_sync_conn_complete sync_conn_complete;
        struct bt_hci_evt_extended_inquiry_result extended_inquiry_result;
        struct bt_hci_evt_encrypt_key_refresh_complete encrypt_key_refresh_complete;
        struct bt_hci_evt_io_capa_req io_capa_req;
        struct bt_hci_evt_io_capa_resp io_capa_resp;
        struct bt_hci_evt_user_confirm_req user_confirm_req;
        struct bt_hci_evt_user_passkey_req user_passkey_req;
        struct bt_hci_evt_ssp_complete ssp_complete;
        struct bt_hci_evt_user_passkey_notify user_passkey_notify;
        struct bt_hci_evt_auth_payload_timeout_exp auth_payload_timeout_exp;
    } evt_data;
} __packed;

struct bt_acl_frame {
    uint8_t h4_type;
    struct bt_hci_acl_hdr acl_hdr;
    struct bt_l2cap_hdr l2cap_hdr;
    union {
        struct {
            struct bt_l2cap_sig_hdr sig_hdr;
            union {
                struct bt_l2cap_cmd_reject cmd_reject;
                struct bt_l2cap_cmd_reject_cid_data cmd_reject_cid_data;
                struct bt_l2cap_conn_req conn_req;
                struct bt_l2cap_conn_rsp conn_rsp;
                struct bt_l2cap_conf_req conf_req;
                struct bt_l2cap_conf_rsp conf_rsp;
                struct bt_l2cap_conf_opt conf_opt;
                struct bt_l2cap_disconn_req disconn_req;
                struct bt_l2cap_disconn_rsp disconn_rsp;
                struct bt_l2cap_info_req info_req;
                struct bt_l2cap_info_rsp info_rsp;
                struct bt_l2cap_conn_param_req conn_param_req;
                struct bt_l2cap_conn_param_rsp conn_param_rsp;
            } l2cap_data;
        };
        struct bt_hidp_data hidp;
    } pl;
} __packed;

struct bt_name_type {
    char name[249];
    int8_t type;
};

struct bt_wii_ext_type {
    uint8_t ext_type[6];
    int8_t type;
};

struct l2cap_chan {
    uint16_t scid;
    uint16_t dcid;
    uint8_t  indent;
};

struct bt_dev {
    int32_t id;
    atomic_t flags;
    uint32_t report_cnt;
    TaskHandle_t xHandle;
    bt_addr_t remote_bdaddr;
    int8_t type;
    uint16_t acl_handle;
    uint8_t l2cap_ident;
    struct l2cap_chan sdp_chan;
    struct l2cap_chan ctrl_chan;
    struct l2cap_chan intr_chan;
};

enum {
    /* BT CTRL flags */
    BT_CTRL_ENABLE,
    BT_CTRL_READY,
    BT_CTRL_PENDING,
    BT_CTRL_NAME_SET,
    BT_CTRL_CLASS_SET,
    BT_CTRL_BDADDR_READ,
    BT_CTRL_VER_READ,
    BT_CTRL_INQUIRY_FILTER,
    BT_CTRL_CONN_FILTER,
    BT_CTRL_PAGE_ENABLE,
    BT_CTRL_INQUIRY,
};

enum {
    /* BT device connection flags */
    BT_DEV_PENDING,
    BT_DEV_DEVICE_FOUND,
    BT_DEV_PAGE,
    BT_DEV_NAME_READ,
    BT_DEV_CONNECTED,
    BT_DEV_AUTHENTICATING,
    BT_DEV_AUTHENTICATED,
    BT_DEV_LINK_KEY_REQ,
    BT_DEV_PIN_CODE_REQ,
    BT_DEV_L2CAP_CONN_REQ,
    BT_DEV_L2CAP_CONNECTED,
    BT_DEV_L2CAP_LCONF_DONE,
    BT_DEV_L2CAP_RCONF_REQ,
    BT_DEV_L2CAP_RCONF_DONE,
    BT_DEV_SDP_PENDING,
    BT_DEV_SDP_CONNECTED,
    BT_DEV_HID_DESCRIPTOR_READ,
    BT_DEV_HID_CTRL_PENDING,
    BT_DEV_HID_CTRL_CONNECTED,
    BT_DEV_HID_INTR_PENDING,
    BT_DEV_HID_INTR_CONNECTED,
    /* HID Conf */
    BT_DEV_WII_LED_SET,
    BT_DEV_WII_STATUS_RX,
    BT_DEV_WII_EXT_CONF_PENDING,
    BT_DEV_WII_EXT_CONF_DONE,
    BT_DEV_WII_EXT_ID_READ,
    BT_DEV_WII_REP_MODE_SET,
};

#define H4_TYPE_COMMAND 1
#define H4_TYPE_ACL     2
#define H4_TYPE_SCO     3
#define H4_TYPE_EVENT   4

static struct bt_dev bt_dev[7] = {0};
TaskHandle_t xHandle;
static struct io input;
static struct io *output;
static struct config *sd_config;
static uint8_t hci_version = 0;
static bt_addr_t local_bdaddr;
static atomic_t bt_flags = 0;
static struct bt_hci_tx_frame bt_hci_tx_frame;
static struct bt_acl_frame bt_acl_frame;
static bt_class_t local_class = {{0x00, 0x01, 0x00}}; /* Computer */
#if 0
static uint16_t min_lx = 0xFFFF;
static uint16_t min_ly = 0xFFFF;
static uint16_t min_rx = 0xFFFF;
static uint16_t min_ry = 0xFFFF;
static uint16_t max_lx = 0;
static uint16_t max_ly = 0;
static uint16_t max_rx = 0;
static uint16_t max_ry = 0;
#endif

static const uint8_t led_dev_id_map[] = {
    0x1, 0x2, 0x4, 0x8, 0x3, 0x6, 0xC
};

static const struct bt_name_type bt_name_type[] = {
    {"Nintendo RVL-CNT-01-UC", WIIU_PRO},
    {"Nintendo RVL-CNT-01-TR", WII_CORE},
    {"Nintendo RVL-CNT-01", WII_CORE}
};

static const struct bt_wii_ext_type bt_wii_ext_type[] = {
    {{0x00, 0x00, 0xA4, 0x20, 0x00, 0x00}, WII_NUNCHUCK},
    {{0x00, 0x00, 0xA4, 0x20, 0x01, 0x01}, WII_CLASSIC},
    {{0x01, 0x00, 0xA4, 0x20, 0x01, 0x01}, WII_CLASSIC}, /* Classic Pro */
    {{0x00, 0x00, 0xA4, 0x20, 0x01, 0x20}, WIIU_PRO}
};

#ifdef H4_TRACE
static void bt_h4_trace(uint8_t *data, uint16_t len, uint8_t dir);
#endif /* H4_TRACE */
static void bt_hci_cmd(uint16_t opcode, uint16_t len);
static void bt_hci_cmd_inquiry(void);
static void bt_hci_cmd_inquiry_cancel(void);
static void bt_hci_cmd_connect(bt_addr_t *bdaddr);
static void bt_hci_cmd_link_key_neg_reply(bt_addr_t bdaddr);
static void bt_hci_cmd_pin_code_reply(bt_addr_t bdaddr, uint8_t pin_len, uint8_t *pin_code);
static void bt_hci_cmd_pin_code_neg_reply(bt_addr_t bdaddr);
static void bt_hci_cmd_auth_requested(uint16_t handle);
static void bt_hci_cmd_remote_name_request(bt_addr_t bdaddr);
static void bt_hci_cmd_reset(void);
static void bt_hci_cmd_write_class_of_device(bt_class_t dev_class);
static void bt_hci_cmd_read_local_version_info(void);
static void bt_hci_cmd_read_bd_addr(void);
static void bt_l2cap_cmd(uint16_t handle, uint16_t cid, uint8_t code, uint8_t ident, uint16_t len);
static void bt_l2cap_cmd_conn_req(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t psm, uint16_t scid);
static void bt_l2cap_cmd_conn_rsp(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t dcid, uint16_t scid, uint16_t result);
static void bt_l2cap_cmd_conf_req(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t dcid);
static void bt_l2cap_cmd_conf_rsp(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t scid);
static void bt_l2cap_cmd_disconn_req(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t dcid, uint16_t scid);
static void bt_l2cap_cmd_disconn_rsp(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t dcid, uint16_t scid);
static void bt_l2cap_cmd_info_rsp(uint16_t handle, uint16_t cid, uint8_t ident, uint16_t type);
static void bt_ctrl_rcv_pkt_ready(void);
static int bt_host_rcv_pkt(uint8_t *data, uint16_t len);
static void bt_hci_event_handler(uint8_t *data, uint16_t len);
static void bt_acl_handler(uint8_t *data, uint16_t len);
static void bt_task(void *param);
static void bt_dev_task(void *param);

static esp_vhci_host_callback_t vhci_host_cb = {
    bt_ctrl_rcv_pkt_ready,
    bt_host_rcv_pkt
};

#ifdef H4_TRACE
static void bt_h4_trace(uint8_t *data, uint16_t len, uint8_t dir) {
    uint8_t col;
    uint16_t byte, line;
    uint16_t line_max = len/16;

    if (len % 16)
        line_max++;

    if (dir)
        printf("I ");
    else
        printf("O ");

    for (byte = 0, line = 0; line < line_max; line++) {
        printf("%06X", byte);
        for (col = 0; col < 16 && byte < len; col++, byte++) {
            printf(" %02X", data[byte]);
        }
        printf("\n");
    }
}
#endif /* H4_TRACE */

static int32_t bt_get_new_dev(struct bt_dev **device) {
    for (uint32_t i = 0; i < 7; i++) {
        if (!atomic_test_bit(&bt_dev[i].flags, BT_DEV_DEVICE_FOUND)) {
            *device = &bt_dev[i];
            return i;
        }
    }
    return -1;
}

static int32_t bt_get_active_dev(struct bt_dev **device) {
    for (uint32_t i = 0; i < 7; i++) {
        if (atomic_test_bit(&bt_dev[i].flags, BT_DEV_DEVICE_FOUND)) {
            *device = &bt_dev[i];
            return i;
        }
    }
    return -1;
}

static int32_t bt_get_dev_from_bdaddr(bt_addr_t *bdaddr, struct bt_dev **device) {
    for (uint32_t i = 0; i < 7; i++) {
        if (memcmp(bdaddr->val, bt_dev[i].remote_bdaddr.val, 6) == 0) {
            *device = &bt_dev[i];
            return i;
        }
    }
    return -1;
}

static int32_t bt_get_dev_from_handle(uint16_t handle, struct bt_dev **device) {
    for (uint32_t i = 0; i < 7; i++) {
        if ((handle & 0xFFF) == bt_dev[i].acl_handle) {
            *device = &bt_dev[i];
            return i;
        }
    }
    return -1;
}

static int32_t bt_get_dev_from_pending_flag(struct bt_dev **device) {
    for (uint32_t i = 0; i < 7; i++) {
        if (atomic_test_bit(&bt_dev[i].flags, BT_DEV_PENDING)) {
            *device = &bt_dev[i];
            return i;
        }
    }
    return -1;
}

static int32_t bt_get_dev_from_scid(uint16_t scid, struct bt_dev **device) {
    *device = &bt_dev[(scid & 0xF)];
    return (scid & 0xF);
}

static int32_t bt_get_type_from_name(const uint8_t* name) {
    for (uint32_t i = 0; i < sizeof(bt_name_type)/sizeof(*bt_name_type); i++) {
        if (memcmp(name, bt_name_type[i].name, strlen(bt_name_type[i].name)) == 0) {
            return bt_name_type[i].type;
        }
    }
    return -1;
}

static int32_t bt_get_type_from_wii_ext(const uint8_t* ext_type) {
    for (uint32_t i = 0; i < sizeof(bt_wii_ext_type)/sizeof(*bt_wii_ext_type); i++) {
        if (memcmp(ext_type, bt_wii_ext_type[i].ext_type, sizeof(bt_wii_ext_type[0].ext_type)) == 0) {
            return bt_wii_ext_type[i].type;
        }
    }
    return -1;
}

static void bt_hci_cmd(uint16_t opcode, uint16_t len) {
    uint16_t buflen = (sizeof(bt_hci_tx_frame.h4_type) + sizeof(bt_hci_tx_frame.cmd_hdr) + len);

    atomic_clear_bit(&bt_flags, BT_CTRL_READY);
    atomic_set_bit(&bt_flags, BT_CTRL_PENDING);

    bt_hci_tx_frame.h4_type = H4_TYPE_COMMAND;

    bt_hci_tx_frame.cmd_hdr.opcode = opcode;
    bt_hci_tx_frame.cmd_hdr.param_len = len;

#ifdef H4_TRACE
    bt_h4_trace((uint8_t *)&bt_hci_tx_frame, buflen, BT_TX);
#endif /* H4_TRACE */

    esp_vhci_host_send_packet((uint8_t *)&bt_hci_tx_frame, buflen);
}

static void bt_hci_cmd_inquiry(void) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_tx_frame.cmd_cp.inquiry.lap[0] = 0x33;
    bt_hci_tx_frame.cmd_cp.inquiry.lap[1] = 0x8B;
    bt_hci_tx_frame.cmd_cp.inquiry.lap[2] = 0x9E;
    bt_hci_tx_frame.cmd_cp.inquiry.length = 0x02; /* 0x02 * 1.28 s = 2.56 s */
    bt_hci_tx_frame.cmd_cp.inquiry.num_rsp = 0xFF;

    bt_hci_cmd(BT_HCI_OP_INQUIRY, sizeof(bt_hci_tx_frame.cmd_cp.inquiry));
}

static void bt_hci_cmd_inquiry_cancel(void) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_INQUIRY_CANCEL, 0);
}

static void bt_hci_cmd_connect(bt_addr_t *bdaddr) {
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&bt_hci_tx_frame.cmd_cp.connect.bdaddr, (void *)bdaddr, sizeof(*bdaddr));
    bt_hci_tx_frame.cmd_cp.connect.packet_type = 0xCC18; /* DH5, DM5, DH3, DM3, DH1 & DM1 */
    bt_hci_tx_frame.cmd_cp.connect.pscan_rep_mode = 0x01; /* R1 */
    bt_hci_tx_frame.cmd_cp.connect.reserved = 0x00;
    bt_hci_tx_frame.cmd_cp.connect.clock_offset = 0x0000;
    bt_hci_tx_frame.cmd_cp.connect.allow_role_switch = 0x00;

    bt_hci_cmd(BT_HCI_OP_CONNECT, sizeof(bt_hci_tx_frame.cmd_cp.connect));
}

static void bt_hci_cmd_accept_conn_req(bt_addr_t *bdaddr) {
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&bt_hci_tx_frame.cmd_cp.accept_conn_req.bdaddr, (void* )bdaddr, sizeof(*bdaddr));
    bt_hci_tx_frame.cmd_cp.accept_conn_req.role = 0x00; /* Become master */

    bt_hci_cmd(BT_HCI_OP_ACCEPT_CONN_REQ, sizeof(bt_hci_tx_frame.cmd_cp.accept_conn_req));
}

static void bt_hci_cmd_link_key_neg_reply(bt_addr_t bdaddr) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_tx_frame.cmd_cp.link_key_neg_reply.bdaddr.val, bdaddr.val, sizeof(bdaddr));

    bt_hci_cmd(BT_HCI_OP_LINK_KEY_NEG_REPLY, sizeof(bt_hci_tx_frame.cmd_cp.link_key_neg_reply));
}

static void bt_hci_cmd_pin_code_reply(bt_addr_t bdaddr, uint8_t pin_len, uint8_t *pin_code) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_tx_frame.cmd_cp.pin_code_reply.bdaddr.val, bdaddr.val, sizeof(bdaddr));
    bt_hci_tx_frame.cmd_cp.pin_code_reply.pin_len = pin_len;
    memcpy(bt_hci_tx_frame.cmd_cp.pin_code_reply.pin_code, pin_code, pin_len);

    bt_hci_cmd(BT_HCI_OP_PIN_CODE_REPLY, sizeof(bt_hci_tx_frame.cmd_cp.pin_code_reply));
}

static void bt_hci_cmd_pin_code_neg_reply(bt_addr_t bdaddr) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_tx_frame.cmd_cp.pin_code_neg_reply.bdaddr.val, bdaddr.val, sizeof(bdaddr));

    bt_hci_cmd(BT_HCI_OP_PIN_CODE_NEG_REPLY, sizeof(bt_hci_tx_frame.cmd_cp.pin_code_neg_reply));
}

static void bt_hci_cmd_auth_requested(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_tx_frame.cmd_cp.auth_requested.handle = handle;

    bt_hci_cmd(BT_HCI_OP_AUTH_REQUESTED, sizeof(bt_hci_tx_frame.cmd_cp.auth_requested));
}

static void bt_hci_cmd_remote_name_request(bt_addr_t bdaddr) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_tx_frame.cmd_cp.remote_name_request.bdaddr.val, bdaddr.val, sizeof(bdaddr));
    bt_hci_tx_frame.cmd_cp.remote_name_request.pscan_rep_mode = 0x01; /* R1 */
    bt_hci_tx_frame.cmd_cp.remote_name_request.reserved = 0x00;
    bt_hci_tx_frame.cmd_cp.remote_name_request.clock_offset = 0x0000;

    bt_hci_cmd(BT_HCI_OP_REMOTE_NAME_REQUEST, sizeof(bt_hci_tx_frame.cmd_cp.remote_name_request));
}

static void bt_hci_cmd_switch_role(bt_addr_t *bdaddr, uint8_t role) {
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&bt_hci_tx_frame.cmd_cp.switch_role.bdaddr, (void* )bdaddr, sizeof(*bdaddr));
    bt_hci_tx_frame.cmd_cp.switch_role.role = role;

    bt_hci_cmd(BT_HCI_OP_SWITCH_ROLE, sizeof(bt_hci_tx_frame.cmd_cp.switch_role));
}

static void bt_hci_cmd_read_link_policy(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_tx_frame.cmd_cp.read_link_policy.handle = handle;

    bt_hci_cmd(BT_HCI_OP_READ_LINK_POLICY, sizeof(bt_hci_tx_frame.cmd_cp.read_link_policy));
}

static void bt_hci_cmd_write_link_policy(uint16_t handle, uint16_t link_policy) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_tx_frame.cmd_cp.write_link_policy.handle = handle;
    bt_hci_tx_frame.cmd_cp.write_link_policy.link_policy = link_policy;

    bt_hci_cmd(BT_HCI_OP_WRITE_LINK_POLICY, sizeof(bt_hci_tx_frame.cmd_cp.write_link_policy));
}

static void bt_hci_cmd_read_default_link_policy(void) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_DEFAULT_LINK_POLICY, 0);
}

static void bt_hci_cmd_write_default_link_policy(uint16_t link_policy) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_tx_frame.cmd_cp.write_default_link_policy.link_policy = link_policy;

    bt_hci_cmd(BT_HCI_OP_WRITE_DEFAULT_LINK_POLICY, sizeof(bt_hci_tx_frame.cmd_cp.write_default_link_policy));
}

static void bt_hci_cmd_reset(void) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_RESET, 0);
}

static void bt_hci_cmd_set_event_filter(struct bt_hci_cp_set_event_filter *event_filter) {
    uint32_t len = 0;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&bt_hci_tx_frame.cmd_cp.set_event_filter, (void *)event_filter, sizeof(bt_hci_tx_frame.cmd_cp.set_event_filter));

    if (event_filter->filter_type == BT_BREDR_FILTER_TYPE_INQUIRY) {
        len += 2;
    }
    else if (event_filter->filter_type == BT_BREDR_FILTER_TYPE_CONN) {
        len += 3;
    }
    if (event_filter->condition_type) {
        len += 6;
    }

    bt_hci_cmd(BT_HCI_OP_SET_EVENT_FILTER, len);
}

static void bt_hci_cmd_write_local_name(void) {
    printf("# %s\n", __FUNCTION__);

    memset((void *)&bt_hci_tx_frame.cmd_cp.write_local_name, 0, sizeof(bt_hci_tx_frame.cmd_cp.write_local_name));
    snprintf((char *)bt_hci_tx_frame.cmd_cp.write_local_name.local_name, sizeof(bt_hci_tx_frame.cmd_cp.write_local_name.local_name),
        "BLUERETRO");

    bt_hci_cmd(BT_HCI_OP_WRITE_LOCAL_NAME, sizeof(bt_hci_tx_frame.cmd_cp.write_local_name));
}

static void bt_hci_cmd_write_scan_enable(uint8_t scan_enable) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_tx_frame.cmd_cp.write_scan_enable.scan_enable = scan_enable;

    bt_hci_cmd(BT_HCI_OP_WRITE_SCAN_ENABLE, sizeof(bt_hci_tx_frame.cmd_cp.write_scan_enable));
}

static void bt_hci_cmd_write_class_of_device(bt_class_t dev_class) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_tx_frame.cmd_cp.write_class_of_device.dev_class.val, dev_class.val, sizeof(dev_class));

    bt_hci_cmd(BT_HCI_OP_WRITE_CLASS_OF_DEVICE, sizeof(bt_hci_tx_frame.cmd_cp.write_class_of_device));
}

static void bt_hci_cmd_read_local_version_info(void) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_VERSION_INFO, 0);
}

static void bt_hci_cmd_read_bd_addr(void) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_BD_ADDR, 0);
}

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

static void bt_hid_cmd(uint16_t handle, uint16_t cid, uint8_t protocol, uint16_t len) {
    uint16_t buflen = (sizeof(bt_acl_frame.h4_type) + sizeof(bt_acl_frame.acl_hdr)
                       + sizeof(bt_acl_frame.l2cap_hdr) + sizeof(bt_acl_frame.pl.hidp.hidp_hdr) + len);

    atomic_clear_bit(&bt_flags, BT_CTRL_READY);

    bt_acl_frame.h4_type = H4_TYPE_ACL;

    bt_acl_frame.acl_hdr.handle = handle | (0x2 << 12); /* BC/PB Flags (2 bits each) */
    bt_acl_frame.acl_hdr.len = buflen - sizeof(bt_acl_frame.h4_type) - sizeof(bt_acl_frame.acl_hdr);

    bt_acl_frame.l2cap_hdr.len = bt_acl_frame.acl_hdr.len - sizeof(bt_acl_frame.l2cap_hdr);
    bt_acl_frame.l2cap_hdr.cid = cid;

    bt_acl_frame.pl.hidp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_acl_frame.pl.hidp.hidp_hdr.protocol = protocol;

#ifdef H4_TRACE
    bt_h4_trace((uint8_t *)&bt_acl_frame, buflen, BT_TX);
#endif /* H4_TRACE */

    esp_vhci_host_send_packet((uint8_t *)&bt_acl_frame, buflen);
}

static void bt_hid_cmd_wii_set_led(uint16_t handle, uint16_t cid, uint8_t conf) {
    //printf("# %s\n", __FUNCTION__);

    bt_acl_frame.pl.hidp.hidp_data.wii_conf.conf = conf;

    bt_hid_cmd(handle, cid, BT_HIDP_WII_LED_REPORT, sizeof(bt_acl_frame.pl.hidp.hidp_data.wii_conf));
}

static void bt_hid_cmd_wii_set_rep_mode(uint16_t handle, uint16_t cid, uint8_t continuous, uint8_t mode) {
    printf("# %s\n", __FUNCTION__);

    bt_acl_frame.pl.hidp.hidp_data.wii_rep_mode.options = (continuous ? 0x04 : 0x00);
    bt_acl_frame.pl.hidp.hidp_data.wii_rep_mode.mode = mode;

    bt_hid_cmd(handle, cid, BT_HIDP_WII_REP_MODE, sizeof(bt_acl_frame.pl.hidp.hidp_data.wii_rep_mode));
}

static void bt_hid_cmd_wii_read(uint16_t handle, uint16_t cid, struct bt_hidp_wii_rd_mem *wii_rd_mem) {
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&bt_acl_frame.pl.hidp.hidp_data.wii_rd_mem, (void *)wii_rd_mem, sizeof(bt_acl_frame.pl.hidp.hidp_data.wii_rd_mem));

    bt_hid_cmd(handle, cid, BT_HIDP_WII_RD_MEM, sizeof(bt_acl_frame.pl.hidp.hidp_data.wii_rd_mem));
}

static void bt_hid_cmd_wii_write(uint16_t handle, uint16_t cid, struct bt_hidp_wii_wr_mem *wii_wr_mem) {
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&bt_acl_frame.pl.hidp.hidp_data.wii_wr_mem, (void *)wii_wr_mem, sizeof(bt_acl_frame.pl.hidp.hidp_data.wii_wr_mem));

    bt_hid_cmd(handle, cid, BT_HIDP_WII_WR_MEM, sizeof(bt_acl_frame.pl.hidp.hidp_data.wii_wr_mem));
}

static void bt_hci_event_handler(uint8_t *data, uint16_t len) {
    struct bt_dev *device = NULL;
    struct bt_hci_rx_frame *bt_hci_rx_frame = (struct bt_hci_rx_frame *)data;

    switch (bt_hci_rx_frame->evt_hdr.evt) {
        case BT_HCI_EVT_INQUIRY_COMPLETE:
            printf("# BT_HCI_EVT_INQUIRY_COMPLETE\n");
            atomic_clear_bit(&bt_flags, BT_CTRL_INQUIRY);
            atomic_clear_bit(&bt_flags, BT_CTRL_PENDING);
            if (!bt_dev[0].flags && xHandle == NULL) {
                xTaskCreatePinnedToCore(&bt_task, "bt_task", 2048, NULL, 5, &xHandle, 0);
            }
            break;
        case BT_HCI_EVT_INQUIRY_RESULT:
            printf("# BT_HCI_EVT_INQUIRY_RESULT\n");
            printf("# Number of responce: %d\n", bt_hci_rx_frame->evt_data.inquiry_result.nb_rsp);
            for (uint8_t i = 1; i <= bt_hci_rx_frame->evt_data.inquiry_result.nb_rsp; i++) {
                bt_get_dev_from_bdaddr((bt_addr_t *)((uint8_t *)&bt_hci_rx_frame->evt_data.inquiry_result + 1 + 6*(i - 1)), &device);
                if (device == NULL) {
                    int32_t bt_dev_id = bt_get_new_dev(&device);
                    if (device) {
                        memcpy(device->remote_bdaddr.val,
                            (uint8_t *)&bt_hci_rx_frame->evt_data.inquiry_result + 1 + 6*(i - 1),
                            sizeof(device->remote_bdaddr));
                        atomic_set_bit(&device->flags, BT_DEV_DEVICE_FOUND);
                        device->id = bt_dev_id;
                        device->ctrl_chan.scid = bt_dev_id | 0x0080;
                        device->intr_chan.scid = bt_dev_id | 0x0090;
                        xTaskCreatePinnedToCore(&bt_dev_task, "bt_dev_task", 2048, device, 5, device->xHandle, 0);
                    }
                }
                printf("# dev: %d Found bdaddr: %02X:%02X:%02X:%02X:%02X:%02X\n", device->id,
                    device->remote_bdaddr.val[5], device->remote_bdaddr.val[4], device->remote_bdaddr.val[3],
                    device->remote_bdaddr.val[2], device->remote_bdaddr.val[1], device->remote_bdaddr.val[0]);
            }
            break;
        case BT_HCI_EVT_CONN_COMPLETE:
            printf("# BT_HCI_EVT_CONN_COMPLETE\n");
            bt_get_dev_from_bdaddr(&bt_hci_rx_frame->evt_data.conn_complete.bdaddr, &device);
            if (!device || bt_hci_rx_frame->evt_data.conn_complete.status) {
                printf("# dev: %d error: 0x%02X\n", device->id ? device->id : -1,
                    bt_hci_rx_frame->evt_data.conn_complete.status);
            }
            else {
                device->acl_handle = bt_hci_rx_frame->evt_data.conn_complete.handle;
                printf("# dev: %d acl_handle: 0x%04X\n", device->id, device->acl_handle);
                atomic_set_bit(&device->flags, BT_DEV_CONNECTED);
                //if (atomic_test_bit(&device->flags, BT_DEV_PAGE)) {
                //    atomic_set_bit(&device->flags, BT_DEV_AUTHENTICATING);
                //}
                atomic_clear_bit(&bt_flags, BT_CTRL_PENDING);
                atomic_clear_bit(&device->flags, BT_DEV_PENDING);
            }
            break;
        case BT_HCI_EVT_CONN_REQUEST:
            printf("# BT_HCI_EVT_CONN_REQUEST\n");
            bt_get_dev_from_bdaddr(&bt_hci_rx_frame->evt_data.conn_request.bdaddr, &device);
            if (device == NULL) {
                int32_t bt_dev_id = bt_get_new_dev(&device);
                if (device) {
                    memcpy(device->remote_bdaddr.val, bt_hci_rx_frame->evt_data.conn_request.bdaddr.val,
                        sizeof(device->remote_bdaddr));
                    device->id = bt_dev_id;
                    device->ctrl_chan.scid = bt_dev_id | 0x0080;
                    device->intr_chan.scid = bt_dev_id | 0x0090;
                    atomic_set_bit(&device->flags, BT_DEV_DEVICE_FOUND);
                    atomic_set_bit(&device->flags, BT_DEV_PAGE);
                    xTaskCreatePinnedToCore(&bt_dev_task, "bt_dev_task", 2048, device, 5, device->xHandle, 0);
                }
            }
            printf("# Page dev: %d bdaddr: %02X:%02X:%02X:%02X:%02X:%02X\n", device->id,
                device->remote_bdaddr.val[5], device->remote_bdaddr.val[4], device->remote_bdaddr.val[3],
                device->remote_bdaddr.val[2], device->remote_bdaddr.val[1], device->remote_bdaddr.val[0]);
            break;
        case BT_HCI_EVT_DISCONN_COMPLETE:
            printf("# BT_HCI_EVT_DISCONN_COMPLETE\n");
            bt_get_dev_from_handle(bt_hci_rx_frame->evt_data.disconn_complete.handle, &device);
            memset(&bt_dev[device->id], 0, sizeof(bt_dev[0]));
            if (bt_get_active_dev(&device) == -1 && xHandle == NULL) {
                printf("# No paired device left, restart inquiry\n");
                xTaskCreatePinnedToCore(&bt_task, "bt_task", 2048, NULL, 5, &xHandle, 0);
            }
            break;
        case BT_HCI_EVT_AUTH_COMPLETE:
            printf("# BT_HCI_EVT_AUTH_COMPLETE\n");
            bt_get_dev_from_handle(bt_hci_rx_frame->evt_data.auth_complete.handle, &device);
            if (bt_hci_rx_frame->evt_data.auth_complete.status) {
                printf("# dev: %d error: 0x%02X\n", device->id,
                    bt_hci_rx_frame->evt_data.auth_complete.status);
            }
            else {
                printf("# dev: %d Pairing done\n", device->id);
                atomic_set_bit(&device->flags, BT_DEV_AUTHENTICATED);
                atomic_clear_bit(&device->flags, BT_DEV_AUTHENTICATING);
            }
            break;
        case BT_HCI_EVT_REMOTE_NAME_REQ_COMPLETE:
            printf("# BT_HCI_EVT_REMOTE_NAME_REQ_COMPLETE:\n");
            bt_get_dev_from_bdaddr(&bt_hci_rx_frame->evt_data.remote_name_req_complete.bdaddr, &device);
            if (bt_hci_rx_frame->evt_data.remote_name_req_complete.status) {
                printf("# dev: %d error: 0x%02X\n", device->id,
                    bt_hci_rx_frame->evt_data.remote_name_req_complete.status);
            }
            else {
                device->type = bt_get_type_from_name(bt_hci_rx_frame->evt_data.remote_name_req_complete.name);
                printf("# dev: %d type: %d %s\n", device->id, device->type, bt_hci_rx_frame->evt_data.remote_name_req_complete.name);
            }
            break;
        case BT_HCI_EVT_CMD_COMPLETE:
            printf("# BT_HCI_EVT_CMD_COMPLETE\n");
            if (bt_hci_rx_frame->evt_data.complete_data.status) {
                printf("# opcode: 0x%04X error: 0x%02X\n",
                    bt_hci_rx_frame->evt_data.complete_hdr.opcode,
                    bt_hci_rx_frame->evt_data.complete_data.status);
            }
            else {
                switch (bt_hci_rx_frame->evt_data.complete_hdr.opcode) {
                    case BT_HCI_OP_INQUIRY_CANCEL:
                        atomic_clear_bit(&bt_flags, BT_CTRL_INQUIRY);
                        break;
                    case BT_HCI_OP_RESET:
                        atomic_set_bit(&bt_flags, BT_CTRL_ENABLE);
                        break;
                    case BT_HCI_OP_SET_EVENT_FILTER:
                        if (atomic_test_bit(&bt_flags, BT_CTRL_INQUIRY_FILTER)) {
                            atomic_set_bit(&bt_flags, BT_CTRL_CONN_FILTER);
                        }
                        else {
                            atomic_set_bit(&bt_flags, BT_CTRL_INQUIRY_FILTER);
                        }
                        break;
                    case BT_HCI_OP_WRITE_LOCAL_NAME:
                        atomic_set_bit(&bt_flags, BT_CTRL_NAME_SET);
                        break;
                    case BT_HCI_OP_WRITE_SCAN_ENABLE:
                        atomic_set_bit(&bt_flags, BT_CTRL_PAGE_ENABLE);
                        break;
                    case BT_HCI_OP_LINK_KEY_NEG_REPLY:
                        bt_get_dev_from_bdaddr(&bt_hci_rx_frame->evt_data.complete_data.link_key_neg_reply.bdaddr, &device);
                        atomic_clear_bit(&device->flags, BT_DEV_LINK_KEY_REQ);
                        break;
                    case BT_HCI_OP_PIN_CODE_REPLY:
                        bt_get_dev_from_bdaddr(&bt_hci_rx_frame->evt_data.complete_data.pin_code_reply.bdaddr, &device);
                        atomic_clear_bit(&device->flags, BT_DEV_PIN_CODE_REQ);
                        break;
                    case BT_HCI_OP_WRITE_CLASS_OF_DEVICE:
                        atomic_set_bit(&bt_flags, BT_CTRL_CLASS_SET);
                        break;
                    case BT_HCI_OP_READ_LOCAL_VERSION_INFO:
                        hci_version = bt_hci_rx_frame->
                            evt_data.complete_data.read_local_version_info.hci_version;
                        printf("# hci_version: %02X\n", hci_version);
                        atomic_set_bit(&bt_flags, BT_CTRL_VER_READ);
                        break;
                    case BT_HCI_OP_READ_BD_ADDR:
                        memcpy(local_bdaddr.val,
                            bt_hci_rx_frame->evt_data.complete_data.read_bd_addr.bdaddr.val,
                            sizeof(local_bdaddr));
                        printf("# local_bdaddr: %02X:%02X:%02X:%02X:%02X:%02X\n",
                            local_bdaddr.val[5], local_bdaddr.val[4], local_bdaddr.val[3],
                            local_bdaddr.val[2], local_bdaddr.val[1], local_bdaddr.val[0]);
                        atomic_set_bit(&bt_flags, BT_CTRL_BDADDR_READ);
                        break;
                }
                atomic_clear_bit(&bt_flags, BT_CTRL_PENDING);
                if (device) {
                    atomic_clear_bit(&device->flags, BT_DEV_PENDING);
                }
            }
            break;
        case BT_HCI_EVT_CMD_STATUS:
            printf("# BT_HCI_EVT_CMD_STATUS\n");
            if (bt_hci_rx_frame->evt_data.cmd_status.status) {
                printf("# opcode: 0x%04X error: 0x%02X\n",
                    bt_hci_rx_frame->evt_data.cmd_status.opcode,
                    bt_hci_rx_frame->evt_data.cmd_status.status);
            }
            else {
                switch (bt_hci_rx_frame->evt_data.cmd_status.opcode) {
                    case BT_HCI_OP_INQUIRY:
                        atomic_set_bit(&bt_flags, BT_CTRL_INQUIRY);
                        break;
                    case BT_HCI_OP_REMOTE_NAME_REQUEST:
                        bt_get_dev_from_pending_flag(&device);
                        if (device) {
                            atomic_set_bit(&device->flags, BT_DEV_NAME_READ);
                            atomic_clear_bit(&device->flags, BT_DEV_PENDING);
                        }
                        break;
                    case BT_HCI_OP_AUTH_REQUESTED:
                        bt_get_dev_from_pending_flag(&device);
                        if (device) {
                            atomic_set_bit(&device->flags, BT_DEV_AUTHENTICATING);
                            atomic_clear_bit(&device->flags, BT_DEV_PENDING);
                        }
                        break;
                }
                atomic_clear_bit(&bt_flags, BT_CTRL_PENDING);
            }
            break;
        case BT_HCI_EVT_PIN_CODE_REQ:
            printf("# BT_HCI_EVT_PIN_CODE_REQ\n");
            bt_get_dev_from_bdaddr(&bt_hci_rx_frame->evt_data.pin_code_req.bdaddr, &device);
            atomic_set_bit(&device->flags, BT_DEV_PIN_CODE_REQ);
            break;
        case BT_HCI_EVT_LINK_KEY_REQ:
            printf("# BT_HCI_EVT_LINK_KEY_REQ\n");
            bt_get_dev_from_bdaddr(&bt_hci_rx_frame->evt_data.link_key_req.bdaddr, &device);
            atomic_set_bit(&device->flags, BT_DEV_LINK_KEY_REQ);
            break;
        case BT_HCI_EVT_LINK_KEY_NOTIFY:
            printf("# BT_HCI_EVT_LINK_KEY_NOTIFY\n");
            break;
    }
}

static void bt_acl_handler(uint8_t *data, uint16_t len) {
    struct bt_dev *device = NULL;
    struct bt_acl_frame *bt_acl_frame = (struct bt_acl_frame *)data;

    switch (bt_acl_frame->pl.sig_hdr.code) {
        case BT_L2CAP_CONN_REQ:
            printf("# BT_L2CAP_CONN_REQ\n");
            bt_get_dev_from_handle(bt_acl_frame->acl_hdr.handle, &device);
            device->l2cap_ident = bt_acl_frame->pl.sig_hdr.ident;
            if (atomic_test_bit(&device->flags, BT_DEV_HID_CTRL_CONNECTED)) {
                device->intr_chan.dcid = bt_acl_frame->pl.l2cap_data.conn_req.scid;
            }
            else {
                device->ctrl_chan.dcid = bt_acl_frame->pl.l2cap_data.conn_req.scid;
                //atomic_set_bit(&device->flags, BT_DEV_AUTHENTICATED);
            }
            atomic_set_bit(&device->flags, BT_DEV_L2CAP_CONN_REQ);
            break;
        case BT_L2CAP_CONN_RSP:
            printf("# BT_L2CAP_CONN_RSP\n");
            bt_get_dev_from_scid(bt_acl_frame->pl.l2cap_data.conn_rsp.scid, &device);
            if (bt_acl_frame->pl.l2cap_data.conn_rsp.result == BT_L2CAP_BR_PENDING) {
                if (bt_acl_frame->pl.l2cap_data.conn_rsp.scid == device->ctrl_chan.scid) {
                    atomic_set_bit(&device->flags, BT_DEV_HID_CTRL_PENDING);
                }
                else {
                    atomic_set_bit(&device->flags, BT_DEV_HID_INTR_PENDING);
                }
            }
            if (bt_acl_frame->pl.l2cap_data.conn_rsp.result == BT_L2CAP_BR_SUCCESS
                && bt_acl_frame->pl.l2cap_data.conn_rsp.status == BT_L2CAP_CS_NO_INFO) {
                device->l2cap_ident = bt_acl_frame->pl.sig_hdr.ident;
                if (bt_acl_frame->pl.l2cap_data.conn_rsp.scid == device->ctrl_chan.scid) {
                    device->ctrl_chan.dcid = bt_acl_frame->pl.l2cap_data.conn_rsp.dcid;
                }
                else {
                    device->intr_chan.dcid = bt_acl_frame->pl.l2cap_data.conn_rsp.dcid;
                }
                atomic_set_bit(&device->flags, BT_DEV_L2CAP_CONNECTED);
                atomic_clear_bit(&bt_flags, BT_CTRL_PENDING);
                atomic_clear_bit(&device->flags, BT_DEV_PENDING);
            }
            break;
        case BT_L2CAP_CONF_REQ:
            printf("# BT_L2CAP_CONF_REQ\n");
            bt_get_dev_from_scid(bt_acl_frame->pl.l2cap_data.conf_req.dcid, &device);
            device->l2cap_ident = bt_acl_frame->pl.sig_hdr.ident;
            atomic_set_bit(&device->flags, BT_DEV_L2CAP_RCONF_REQ);
            break;
        case BT_L2CAP_CONF_RSP:
            printf("# BT_L2CAP_CONF_RSP\n");
            bt_get_dev_from_scid(bt_acl_frame->pl.l2cap_data.conf_rsp.scid, &device);
            device->l2cap_ident = bt_acl_frame->pl.sig_hdr.ident;
            atomic_set_bit(&device->flags, BT_DEV_L2CAP_LCONF_DONE);
            atomic_clear_bit(&bt_flags, BT_CTRL_PENDING);
            atomic_clear_bit(&device->flags, BT_DEV_PENDING);
            break;
        case BT_L2CAP_DISCONN_REQ:
            printf("# BT_L2CAP_DISCONN_REQ\n");
            break;
        case BT_HIDP_DATA_IN:
            bt_get_dev_from_scid(bt_acl_frame->l2cap_hdr.cid, &device);
            switch (bt_acl_frame->pl.hidp.hidp_hdr.protocol) {
                case BT_HIDP_WII_STATUS:
                    printf("# BT_HIDP_WII_STATUS\n");
                    atomic_set_bit(&device->flags, BT_DEV_WII_STATUS_RX);
                    atomic_clear_bit(&device->flags, BT_DEV_WII_EXT_CONF_PENDING);
                    atomic_clear_bit(&device->flags, BT_DEV_WII_EXT_CONF_DONE);
                    atomic_clear_bit(&device->flags, BT_DEV_WII_REP_MODE_SET);
                    if ((bt_acl_frame->pl.hidp.hidp_data.wii_status.flags & BT_HIDP_WII_FLAGS_EXT_CONN) && device->type == WII_CORE) {
                        printf("# dev: %d Skip ext id ext: %d type: %d\n", device->id,
                            (bt_acl_frame->pl.hidp.hidp_data.wii_status.flags & BT_HIDP_WII_FLAGS_EXT_CONN), device->type);
                        atomic_clear_bit(&device->flags, BT_DEV_WII_EXT_ID_READ);
                    }
                    else {
                        atomic_set_bit(&device->flags, BT_DEV_WII_EXT_ID_READ);
                    }
                    if (device->xHandle == NULL) {
                        printf("# dev: %d respawn config thread\n", device->id);
                        xTaskCreatePinnedToCore(&bt_dev_task, "bt_dev_task", 2048, device, 5, device->xHandle, 0);
                    }
                    break;
                case BT_HIDP_WII_RD_DATA:
                    printf("# BT_HIDP_WII_RD_DATA\n");
                    device->type = bt_get_type_from_wii_ext(bt_acl_frame->pl.hidp.hidp_data.wii_rd_data.data);
                    printf("# dev: %d wii ext: %d\n", device->id, device->type);
                    atomic_clear_bit(&device->flags, BT_DEV_PENDING);
                    break;
                case BT_HIDP_WII_ACK:
                    printf("# BT_HIDP_WII_ACK\n");
                    switch(bt_acl_frame->pl.hidp.hidp_data.wii_ack.report) {
                        case BT_HIDP_WII_WR_MEM:
                            atomic_clear_bit(&device->flags, BT_DEV_PENDING);
                            break;
                    }
                    if (bt_acl_frame->pl.hidp.hidp_data.wii_ack.err) {
                        printf("# dev: %d ack err: 0x%02X\n", device->id, bt_acl_frame->pl.hidp.hidp_data.wii_ack.err);
                    }
                    break;
                case BT_HIDP_WII_CORE_ACC_EXT:
                {
                    struct wiiu_pro_map *wiiu_pro = (struct wiiu_pro_map *)&bt_acl_frame->pl.hidp.hidp_data.wii_core_acc_ext.ext;
                    device->report_cnt++;
                    input.format = device->type;
                    if (atomic_test_bit(&bt_flags, BT_CTRL_READY) && atomic_test_bit(&input.flags, BTIO_UPDATE_CTRL)) {
                        bt_hid_cmd_wii_set_led(device->acl_handle, device->intr_chan.dcid, input.leds_rumble);
                        atomic_clear_bit(&input.flags, BTIO_UPDATE_CTRL);
                    }
                    memcpy(&input.io.wiiu_pro, wiiu_pro, sizeof(*wiiu_pro));
                    translate_status(sd_config, &input, output);
#if 0
                    min_lx = min(min_lx, wiiu_pro->axes[0]);
                    min_ly = min(min_ly, wiiu_pro->axes[2]);
                    min_rx = min(min_rx, wiiu_pro->axes[1]);
                    min_ry = min(min_ry, wiiu_pro->axes[3]);
                    max_lx = max(max_lx, wiiu_pro->axes[0]);
                    max_ly = max(max_ly, wiiu_pro->axes[2]);
                    max_rx = max(max_rx, wiiu_pro->axes[1]);
                    max_ry = max(max_ry, wiiu_pro->axes[3]);
                    printf("JG2019 MIN LX 0x%04X LY 0x%04X RX 0x%04X RY 0x%04X\n", min_lx, min_ly, min_rx, min_ry);
                    printf("JG2019 MAX LX 0x%04X LY 0x%04X RX 0x%04X RY 0x%04X\n", max_lx, max_ly, max_rx, max_ry);
                    printf("JG2019 %04X %04X %04X %04X\n", wiiu_pro->axes[0] - 0x800, wiiu_pro->axes[2] - 0x800, wiiu_pro->axes[1] - 0x800, wiiu_pro->axes[3] - 0x800);
#endif
                    break;
                }
            }
            break;
    }
}

/*
 * @brief: BT controller callback function, used to notify the upper layer that
 *         controller is ready to receive command
 */
static void bt_ctrl_rcv_pkt_ready(void) {
    atomic_set_bit(&bt_flags, BT_CTRL_READY);
}

/*
 * @brief: BT controller callback function, to transfer data packet to upper
 *         controller is ready to receive command
 */
static int bt_host_rcv_pkt(uint8_t *data, uint16_t len) {
#ifdef H4_TRACE
    bt_h4_trace(data, len, BT_RX);
#endif /* H4_TRACE */

    switch(data[0]) {
        case H4_TYPE_COMMAND:
            break;
        case H4_TYPE_ACL:
            bt_acl_handler(data, len);
            break;
        case H4_TYPE_SCO:
            break;
        case H4_TYPE_EVENT:
            bt_hci_event_handler(data, len);
            break;
    }

    return 0;
}

static void bt_task(void *param) {
    while (1) {
        if (atomic_test_bit(&bt_flags, BT_CTRL_READY)) {
            if (!atomic_test_bit(&bt_flags, BT_CTRL_PENDING)) {
                if (atomic_test_bit(&bt_flags, BT_CTRL_ENABLE)) {
                    if (!atomic_test_bit(&bt_flags, BT_CTRL_NAME_SET)) {
                        bt_hci_cmd_write_local_name();
                    }
                    else if (!atomic_test_bit(&bt_flags, BT_CTRL_CLASS_SET)) {
                        bt_hci_cmd_write_class_of_device(local_class);
                    }
                    else if (!atomic_test_bit(&bt_flags, BT_CTRL_BDADDR_READ)) {
                        bt_hci_cmd_read_bd_addr();
                    }
                    else if (!atomic_test_bit(&bt_flags, BT_CTRL_VER_READ)) {
                        bt_hci_cmd_read_local_version_info();
                    }
                    else if (!atomic_test_bit(&bt_flags, BT_CTRL_INQUIRY_FILTER)) {
                        struct bt_hci_cp_set_event_filter event_filter = {
                            .filter_type = BT_BREDR_FILTER_TYPE_INQUIRY,
                            .condition_type = BT_BDEDR_COND_TYPE_CLASS,
                            .inquiry_class.dev_class = {0x00, 0x05, 0x00},
                            .inquiry_class.dev_class_mask = {0x00, 0x1F, 0x00}
                        };

                        bt_hci_cmd_set_event_filter(&event_filter);
                    }
                    else if (!atomic_test_bit(&bt_flags, BT_CTRL_CONN_FILTER)) {
                        struct bt_hci_cp_set_event_filter event_filter = {
                            .filter_type = BT_BREDR_FILTER_TYPE_CONN,
                            .condition_type = BT_BDEDR_COND_TYPE_CLASS,
                            .conn_class.dev_class = {0x00, 0x05, 0x00},
                            .conn_class.dev_class_mask = {0x00, 0x1F, 0x00},
                            .conn_class.auto_accept_flag =  BT_BREDR_AUTO_OFF
                        };

                        bt_hci_cmd_set_event_filter(&event_filter);
                    }
                    else if (!atomic_test_bit(&bt_flags, BT_CTRL_PAGE_ENABLE)) {
                        bt_hci_cmd_write_scan_enable(BT_BREDR_SCAN_PAGE);
                    }
                    else if (!atomic_test_bit(&bt_flags, BT_CTRL_INQUIRY)) {
                        vTaskDelay(2560 / portTICK_PERIOD_MS); /* Stay in page scan as much as in inquiry */
                        bt_hci_cmd_inquiry();
                        xHandle = NULL;
                        vTaskDelete(NULL);
                    }
                }
                else {
                    bt_hci_cmd_reset();
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

static void bt_dev_task(void *param) {
    struct bt_dev *device = (struct bt_dev *)param;

    while (1) {
        if (atomic_test_bit(&bt_flags, BT_CTRL_READY)) {
            if (!atomic_test_bit(&bt_flags, BT_CTRL_PENDING)) {
                if (!atomic_test_bit(&device->flags, BT_DEV_PENDING)) {
                    if (atomic_test_bit(&device->flags, BT_DEV_CONNECTED)) {
                        if (atomic_test_bit(&device->flags, BT_DEV_AUTHENTICATED) || atomic_test_bit(&device->flags, BT_DEV_PAGE)) {
                            if (!atomic_test_bit(&device->flags, BT_DEV_HID_CTRL_CONNECTED)) {
                                if (!atomic_test_bit(&device->flags, BT_DEV_L2CAP_CONNECTED)) {
                                    if (atomic_test_bit(&device->flags, BT_DEV_PAGE)) {
                                        if (atomic_test_bit(&device->flags, BT_DEV_L2CAP_CONN_REQ)) {
                                            bt_l2cap_cmd_conn_rsp(device->acl_handle, BT_L2CAP_CID_BR_SIG, device->l2cap_ident, device->ctrl_chan.scid, device->ctrl_chan.dcid, 0);
                                            atomic_set_bit(&device->flags, BT_DEV_L2CAP_CONNECTED);
                                        }
                                    }
                                    else {
                                        device->l2cap_ident = 0x00;
                                        atomic_set_bit(&device->flags, BT_DEV_PENDING);
                                        bt_l2cap_cmd_conn_req(device->acl_handle, BT_L2CAP_CID_BR_SIG, device->l2cap_ident, BT_L2CAP_PSM_HID_CTRL, device->ctrl_chan.scid);
                                    }
                                }
                                else if (atomic_test_bit(&device->flags, BT_DEV_L2CAP_RCONF_REQ)) {
                                    bt_l2cap_cmd_conf_rsp(device->acl_handle, BT_L2CAP_CID_BR_SIG, device->l2cap_ident, device->ctrl_chan.dcid);
                                    atomic_clear_bit(&device->flags, BT_DEV_L2CAP_RCONF_REQ);
                                    atomic_set_bit(&device->flags, BT_DEV_L2CAP_RCONF_DONE);
                                }
                                else if (!atomic_test_bit(&device->flags, BT_DEV_L2CAP_LCONF_DONE)) {
                                    device->l2cap_ident++;
                                    atomic_set_bit(&device->flags, BT_DEV_PENDING);
                                    bt_l2cap_cmd_conf_req(device->acl_handle, BT_L2CAP_CID_BR_SIG, device->l2cap_ident, device->ctrl_chan.dcid);
                                }
                                else if (atomic_test_bit(&device->flags, BT_DEV_L2CAP_RCONF_DONE)) {
                                    atomic_clear_bit(&device->flags, BT_DEV_L2CAP_CONN_REQ);
                                    atomic_clear_bit(&device->flags, BT_DEV_L2CAP_CONNECTED);
                                    atomic_clear_bit(&device->flags, BT_DEV_L2CAP_LCONF_DONE);
                                    atomic_clear_bit(&device->flags, BT_DEV_L2CAP_RCONF_DONE);
                                    atomic_set_bit(&device->flags, BT_DEV_HID_CTRL_CONNECTED);
                                }
                            }
                            else if (!atomic_test_bit(&device->flags, BT_DEV_HID_INTR_CONNECTED)) {
                                if (!atomic_test_bit(&device->flags, BT_DEV_L2CAP_CONNECTED)) {
                                    if (atomic_test_bit(&device->flags, BT_DEV_PAGE)) {
                                        if (atomic_test_bit(&device->flags, BT_DEV_L2CAP_CONN_REQ)) {
                                            bt_l2cap_cmd_conn_rsp(device->acl_handle, BT_L2CAP_CID_BR_SIG, device->l2cap_ident, device->intr_chan.scid, device->intr_chan.dcid, 0);
                                            atomic_set_bit(&device->flags, BT_DEV_L2CAP_CONNECTED);
                                        }
                                    }
                                    else {
                                        device->l2cap_ident++;
                                        atomic_set_bit(&device->flags, BT_DEV_PENDING);
                                        bt_l2cap_cmd_conn_req(device->acl_handle, BT_L2CAP_CID_BR_SIG, device->l2cap_ident, BT_L2CAP_PSM_HID_INTR, device->intr_chan.scid);
                                    }
                                }
                                else if (atomic_test_bit(&device->flags, BT_DEV_L2CAP_RCONF_REQ)) {
                                    bt_l2cap_cmd_conf_rsp(device->acl_handle, BT_L2CAP_CID_BR_SIG, device->l2cap_ident, device->intr_chan.dcid);
                                    atomic_clear_bit(&device->flags, BT_DEV_L2CAP_RCONF_REQ);
                                    atomic_set_bit(&device->flags, BT_DEV_L2CAP_RCONF_DONE);
                                }
                                else if (!atomic_test_bit(&device->flags, BT_DEV_L2CAP_LCONF_DONE)) {
                                    device->l2cap_ident++;
                                    atomic_set_bit(&device->flags, BT_DEV_PENDING);
                                    bt_l2cap_cmd_conf_req(device->acl_handle, BT_L2CAP_CID_BR_SIG, device->l2cap_ident, device->intr_chan.dcid);
                                }
                                else if (atomic_test_bit(&device->flags, BT_DEV_L2CAP_RCONF_DONE)) {
                                    atomic_clear_bit(&device->flags, BT_DEV_L2CAP_CONNECTED);
                                    atomic_clear_bit(&device->flags, BT_DEV_L2CAP_LCONF_DONE);
                                    atomic_clear_bit(&device->flags, BT_DEV_L2CAP_RCONF_DONE);
                                    atomic_set_bit(&device->flags, BT_DEV_HID_INTR_CONNECTED);
                                }
                            }
                            else {
                                /* HID report config */
                                if (atomic_test_bit(&device->flags, BT_DEV_WII_STATUS_RX)) {
                                    if (atomic_test_bit(&device->flags, BT_DEV_WII_EXT_ID_READ)) {
                                        bt_hid_cmd_wii_set_rep_mode(device->acl_handle, device->intr_chan.dcid, 1, BT_HIDP_WII_CORE_ACC_EXT);
                                        atomic_set_bit(&device->flags, BT_DEV_WII_REP_MODE_SET);
                                        device->xHandle = NULL;
                                        vTaskDelete(NULL);
                                    }
                                    else if (!atomic_test_bit(&device->flags, BT_DEV_WII_EXT_CONF_PENDING)) {
                                        struct bt_hidp_wii_wr_mem wii_wr_mem =
                                        {
                                            BT_HIDP_WII_REG,
                                            {0xA4, 0x00, 0xF0},
                                            0x01,
                                            {0x55}
                                        };
                                        atomic_set_bit(&device->flags, BT_DEV_PENDING);
                                        bt_hid_cmd_wii_write(device->acl_handle, device->intr_chan.dcid, &wii_wr_mem);
                                        atomic_set_bit(&device->flags, BT_DEV_WII_EXT_CONF_PENDING);
                                    }
                                    else if (!atomic_test_bit(&device->flags, BT_DEV_WII_EXT_CONF_DONE)) {
                                        struct bt_hidp_wii_wr_mem wii_wr_mem =
                                        {
                                            BT_HIDP_WII_REG,
                                            {0xA4, 0x00, 0xFB},
                                            0x01,
                                            {0x00}
                                        };
                                        atomic_set_bit(&device->flags, BT_DEV_PENDING);
                                        bt_hid_cmd_wii_write(device->acl_handle, device->intr_chan.dcid, &wii_wr_mem);
                                        atomic_set_bit(&device->flags, BT_DEV_WII_EXT_CONF_DONE);
                                    }
                                    else {
                                        struct bt_hidp_wii_rd_mem wii_rd_mem =
                                        {
                                            BT_HIDP_WII_REG,
                                            {0xA4, 0x00, 0xFA},
                                            0x0600,
                                        };
                                        atomic_set_bit(&device->flags, BT_DEV_PENDING);
                                        bt_hid_cmd_wii_read(device->acl_handle, device->intr_chan.dcid, &wii_rd_mem);
                                        atomic_set_bit(&device->flags, BT_DEV_WII_EXT_ID_READ);
                                    }
                                }
                                else if (!atomic_test_bit(&device->flags, BT_DEV_WII_LED_SET)) {
                                    bt_hid_cmd_wii_set_led(device->acl_handle, device->intr_chan.dcid, led_dev_id_map[device->id] << 4);
                                    atomic_set_bit(&device->flags, BT_DEV_WII_LED_SET);
                                }
                                else {
                                    bt_hid_cmd_wii_set_rep_mode(device->acl_handle, device->intr_chan.dcid, 1, BT_HIDP_WII_CORE_ACC_EXT);
                                    atomic_set_bit(&device->flags, BT_DEV_WII_REP_MODE_SET);
                                    device->xHandle = NULL;
                                    vTaskDelete(NULL);
                                }
                            }
                        }
                        else if (!atomic_test_bit(&device->flags, BT_DEV_NAME_READ)) {
                            atomic_set_bit(&device->flags, BT_DEV_PENDING);
                            bt_hci_cmd_remote_name_request(device->remote_bdaddr);
                        }
                        else if (!atomic_test_bit(&device->flags, BT_DEV_AUTHENTICATING)) {
                            atomic_set_bit(&device->flags, BT_DEV_PENDING);
                            bt_hci_cmd_auth_requested(device->acl_handle);
                        }
                        else if (atomic_test_bit(&device->flags, BT_DEV_LINK_KEY_REQ)) {
                            atomic_set_bit(&device->flags, BT_DEV_PENDING);
                            bt_hci_cmd_link_key_neg_reply(device->remote_bdaddr);
                        }
                        else if (atomic_test_bit(&device->flags, BT_DEV_PIN_CODE_REQ)) {
                            atomic_set_bit(&device->flags, BT_DEV_PENDING);
                            if (device->type == WII_CORE) {
                                bt_hci_cmd_pin_code_reply(device->remote_bdaddr, 6, device->remote_bdaddr.val);
                            }
                            else {
                                bt_hci_cmd_pin_code_reply(device->remote_bdaddr, 6, local_bdaddr.val);
                            }
                        }
                    }
                    else {
                        atomic_set_bit(&device->flags, BT_DEV_PENDING);
                        if (atomic_test_bit(&device->flags, BT_DEV_PAGE)) {
                            bt_hci_cmd_accept_conn_req(&device->remote_bdaddr);
                        }
                        else {
                            bt_hci_cmd_connect(&device->remote_bdaddr);
                        }
                    }
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

esp_err_t bt_init(struct io *io_data, struct config *config) {
    output = io_data;
    sd_config = config;

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        printf("Bluetooth controller initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        printf("Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_vhci_host_register_callback(&vhci_host_cb);

    bt_ctrl_rcv_pkt_ready();

    xTaskCreatePinnedToCore(&bt_task, "bt_task", 2048, NULL, 5, &xHandle, 0);
    return ret;
}

