#include <stdio.h>
#include "bt_host.h"
#include "bt_l2cap.h"
#include "bt_hidp_wii.h"
#include "bt_hci.h"
#include "util.h"

#define BT_INQUIRY_MAX 10

typedef void (*bt_cmd_func_t)(void *param);

struct bt_name_type {
    char name[249];
    int8_t type;
};

struct bt_hci_cmd_cp {
    bt_cmd_func_t cmd;
    void *cp;
};

static const char bt_default_pin[][5] = {
    "0000",
    "1234",
    "1111",
};

static const struct bt_name_type bt_name_type[] = {
    {"PLAYSTATION(R)3 Controller", PS3_DS3},
    {"Nintendo RVL-CNT-01-UC", WIIU_PRO}, /* Must be before WII_CORE */
    {"Nintendo RVL-CNT-01-TR", WII_CORE},
    {"Nintendo RVL-CNT-01", WII_CORE},
    {"Wireless Controller", PS4_DS4},
    {"Xbox Wireless Controller", XB1_S},
    {"Pro Controller", SWITCH_PRO},
};

static const struct bt_hci_cp_set_event_filter clr_evt_filter = {
    .filter_type = BT_BREDR_FILTER_TYPE_CLEAR,
};

static const struct bt_hci_cp_set_event_filter inquiry_evt_filter = {
    .filter_type = BT_BREDR_FILTER_TYPE_INQUIRY,
    .condition_type = BT_BDEDR_COND_TYPE_CLASS,
    .inquiry_class.dev_class = {0x00, 0x05, 0x00},
    .inquiry_class.dev_class_mask = {0x00, 0x1F, 0x00},
};

static const struct bt_hci_cp_set_event_filter conn_evt_filter = {
    .filter_type = BT_BREDR_FILTER_TYPE_CONN,
    .condition_type = BT_BDEDR_COND_TYPE_CLASS,
    .conn_class.dev_class = {0x00, 0x05, 0x00},
    .conn_class.dev_class_mask = {0x00, 0x1F, 0x00},
    .conn_class.auto_accept_flag =  BT_BREDR_AUTO_OFF,
};

static uint32_t bt_hci_pkt_retry = 0;
static uint32_t bt_nb_inquiry = 0;
static uint8_t local_bdaddr[6];
static uint32_t bt_config_state = 0;

static int32_t bt_hci_get_type_from_name(const uint8_t* name);
static void bt_hci_cmd(uint16_t opcode, uint32_t cp_len);
//static void bt_hci_cmd_inquiry(void *cp);
//static void bt_hci_cmd_inquiry_cancel(void *cp);
static void bt_hci_cmd_periodic_inquiry(void *cp);
static void bt_hci_cmd_exit_periodic_inquiry(void *cp);
static void bt_hci_cmd_connect(void *bdaddr);
static void bt_hci_cmd_accept_conn_req(void *bdaddr);
static void bt_hci_cmd_link_key_neg_reply(void *bdaddr);
static void bt_hci_cmd_pin_code_reply(void *cp);
//static void bt_hci_cmd_pin_code_neg_reply(void *bdaddr);
static void bt_hci_cmd_auth_requested(void *handle);
static void bt_hci_cmd_set_conn_encrypt(void *handle);
static void bt_hci_cmd_remote_name_request(void *bdaddr);
static void bt_hci_cmd_read_remote_features(void *handle);
static void bt_hci_cmd_read_remote_ext_features(void *handle);
static void bt_hci_cmd_io_capability_reply(void *bdaddr);
static void bt_hci_cmd_user_confirm_reply(void *bdaddr);
//static void bt_hci_cmd_switch_role(void *cp);
//static void bt_hci_cmd_read_link_policy(void *handle);
//static void bt_hci_cmd_write_link_policy(void *cp);
//static void bt_hci_cmd_read_default_link_policy(void *cp);
static void bt_hci_cmd_write_default_link_policy(void *cp);
static void bt_hci_cmd_set_event_mask(void *cp);
static void bt_hci_cmd_reset(void *cp);
static void bt_hci_cmd_set_event_filter(void *cp);
static void bt_hci_cmd_read_stored_link_key(void *cp);
static void bt_hci_cmd_delete_stored_link_key(void *cp);
static void bt_hci_cmd_write_local_name(void *cp);
static void bt_hci_cmd_read_local_name(void *cp);
static void bt_hci_cmd_write_conn_accept_timeout(void *cp);
static void bt_hci_cmd_write_page_scan_timeout(void *cp);
static void bt_hci_cmd_write_scan_enable(void *cp);
static void bt_hci_cmd_write_page_scan_activity(void *cp);
static void bt_hci_cmd_write_inquiry_scan_activity(void *cp);
static void bt_hci_cmd_write_auth_enable(void *cp);
static void bt_hci_cmd_read_page_scan_activity(void *cp);
static void bt_hci_cmd_read_class_of_device(void *cp);
static void bt_hci_cmd_write_class_of_device(void *cp);
static void bt_hci_cmd_read_voice_setting(void *cp);
static void bt_hci_cmd_write_hold_mode_act(void *cp);
static void bt_hci_cmd_read_num_supported_iac(void *cp);
static void bt_hci_cmd_read_current_iac_lap(void *cp);
static void bt_hci_cmd_write_inquiry_mode(void *cp);
static void bt_hci_cmd_read_page_scan_type(void *cp);
static void bt_hci_cmd_write_page_scan_type(void *cp);
static void bt_hci_cmd_write_ssp_mode(void *cp);
static void bt_hci_cmd_read_inquiry_rsp_tx_pwr_lvl(void *cp);
static void bt_hci_cmd_write_le_host_supp(void *cp);
static void bt_hci_cmd_read_local_version_info(void *cp);
static void bt_hci_cmd_read_supported_commands(void *cp);
static void bt_hci_cmd_read_local_features(void *cp);
static void bt_hci_cmd_read_local_ext_features(void *cp);
static void bt_hci_cmd_read_buffer_size(void *cp);
static void bt_hci_cmd_read_bd_addr(void *cp);
//static void bt_hci_cmd_read_data_block_size(void *cp);
//static void bt_hci_cmd_read_local_codecs(void *cp);
//static void bt_hci_cmd_read_local_sp_options(void *cp);

static const struct bt_hci_cmd_cp bt_hci_config[] = {
    {bt_hci_cmd_reset, NULL},
    {bt_hci_cmd_read_local_features, NULL},
    {bt_hci_cmd_read_local_version_info, NULL},
    {bt_hci_cmd_read_bd_addr, NULL},
    {bt_hci_cmd_read_buffer_size, NULL},
    {bt_hci_cmd_read_class_of_device, NULL},
    {bt_hci_cmd_read_local_name, NULL},
    {bt_hci_cmd_read_voice_setting, NULL},
    {bt_hci_cmd_read_num_supported_iac, NULL},
    {bt_hci_cmd_read_current_iac_lap, NULL},
    {bt_hci_cmd_set_event_filter, (void *)&clr_evt_filter},
    {bt_hci_cmd_write_conn_accept_timeout, NULL},
    {bt_hci_cmd_read_supported_commands, NULL},
    {bt_hci_cmd_write_ssp_mode, NULL},
    {bt_hci_cmd_write_inquiry_mode, NULL},
    {bt_hci_cmd_read_inquiry_rsp_tx_pwr_lvl, NULL},
    {bt_hci_cmd_read_local_ext_features, NULL},
    {bt_hci_cmd_read_stored_link_key, NULL},
    {bt_hci_cmd_read_page_scan_activity, NULL},
    {bt_hci_cmd_read_page_scan_type, NULL},
    {bt_hci_cmd_write_le_host_supp, NULL},
    {bt_hci_cmd_delete_stored_link_key, NULL},
    {bt_hci_cmd_write_class_of_device, NULL},
    {bt_hci_cmd_write_local_name, NULL},
    {bt_hci_cmd_set_event_filter, (void *)&inquiry_evt_filter},
    {bt_hci_cmd_set_event_filter, (void *)&conn_evt_filter},
    {bt_hci_cmd_write_auth_enable, NULL},
    {bt_hci_cmd_set_event_mask, NULL},
    {bt_hci_cmd_write_page_scan_activity, NULL},
    {bt_hci_cmd_write_inquiry_scan_activity, NULL},
    {bt_hci_cmd_write_page_scan_type, NULL},
    {bt_hci_cmd_write_page_scan_timeout, NULL},
    {bt_hci_cmd_write_hold_mode_act, NULL},
    {bt_hci_cmd_write_scan_enable, NULL},
    {bt_hci_cmd_write_default_link_policy, NULL},
    {bt_hci_cmd_periodic_inquiry, NULL},
};

static int32_t bt_hci_get_type_from_name(const uint8_t* name) {
    for (uint32_t i = 0; i < sizeof(bt_name_type)/sizeof(*bt_name_type); i++) {
        if (memcmp(name, bt_name_type[i].name, strlen(bt_name_type[i].name)) == 0) {
            return bt_name_type[i].type;
        }
    }
    return -1;
}

static void bt_hci_q_conf(uint32_t next) {
    if (next) {
        bt_config_state++;
    }
    if (bt_config_state < ARRAY_SIZE(bt_hci_config)) {
        bt_hci_config[bt_config_state].cmd(bt_hci_config[bt_config_state].cp);
    }
}

static void bt_hci_cmd(uint16_t opcode, uint32_t cp_len) {
    uint32_t packet_len = BT_HCI_H4_HDR_SIZE + BT_HCI_CMD_HDR_SIZE + cp_len;

    bt_hci_pkt_tmp.h4_hdr.type = BT_HCI_H4_TYPE_CMD;

    bt_hci_pkt_tmp.cmd_hdr.opcode = opcode;
    bt_hci_pkt_tmp.cmd_hdr.param_len = cp_len;

    bt_host_txq_add((uint8_t *)&bt_hci_pkt_tmp, packet_len);
}

#if 0
static void bt_hci_cmd_inquiry(void *cp) {
    struct bt_hci_cp_inquiry *inquiry = (struct bt_hci_cp_inquiry *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    inquiry->lap[0] = 0x33;
    inquiry->lap[1] = 0x8B;
    inquiry->lap[2] = 0x9E;
    inquiry->length = 0x20;
    inquiry->num_rsp = 0xFF;

    bt_hci_cmd(BT_HCI_OP_INQUIRY, sizeof(*inquiry));
}

static void bt_hci_cmd_inquiry_cancel(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_INQUIRY_CANCEL, 0);
}
#endif

static void bt_hci_cmd_periodic_inquiry(void *cp) {
    struct bt_hci_cp_periodic_inquiry *periodic_inquiry = (struct bt_hci_cp_periodic_inquiry *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    periodic_inquiry->max_period_length = 0x0A;
    periodic_inquiry->min_period_length = 0x08;
    periodic_inquiry->lap[0] = 0x33;
    periodic_inquiry->lap[1] = 0x8B;
    periodic_inquiry->lap[2] = 0x9E;
    periodic_inquiry->length = 0x03; /* 0x03 * 1.28 s = 3.84 s */
    periodic_inquiry->num_rsp = 0xFF;

    bt_hci_cmd(BT_HCI_OP_PERIODIC_INQUIRY, sizeof(*periodic_inquiry));
}

static void bt_hci_cmd_exit_periodic_inquiry(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_EXIT_PERIODIC_INQUIRY, 0);
}

static void bt_hci_cmd_connect(void *bdaddr) {
    struct bt_hci_cp_connect *connect = (struct bt_hci_cp_connect *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&connect->bdaddr, bdaddr, sizeof(connect->bdaddr));
    connect->packet_type = 0xCC18; /* DH5, DM5, DH3, DM3, DH1 & DM1 */
    connect->pscan_rep_mode = 0x00; /* R1 */
    connect->reserved = 0x00;
    connect->clock_offset = 0x0000;
    connect->allow_role_switch = 0x01;

    bt_hci_cmd(BT_HCI_OP_CONNECT, sizeof(*connect));
}

static void bt_hci_cmd_accept_conn_req(void *bdaddr) {
    struct bt_hci_cp_accept_conn_req *accept_conn_req = (struct bt_hci_cp_accept_conn_req *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&accept_conn_req->bdaddr, bdaddr, sizeof(accept_conn_req->bdaddr));
    accept_conn_req->role = 0x00; /* Become master */

    bt_hci_cmd(BT_HCI_OP_ACCEPT_CONN_REQ, sizeof(*accept_conn_req));
}

static void bt_hci_cmd_link_key_reply(void *cp) {
    struct bt_hci_cp_link_key_reply *link_key_reply = (struct bt_hci_cp_link_key_reply *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)link_key_reply, cp, sizeof(*link_key_reply));

    bt_hci_cmd(BT_HCI_OP_LINK_KEY_REPLY, sizeof(*link_key_reply));
}

static void bt_hci_cmd_link_key_neg_reply(void *bdaddr) {
    struct bt_hci_cp_link_key_neg_reply *link_key_neg_reply = (struct bt_hci_cp_link_key_neg_reply *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&link_key_neg_reply->bdaddr, bdaddr, sizeof(link_key_neg_reply->bdaddr));

    bt_hci_cmd(BT_HCI_OP_LINK_KEY_NEG_REPLY, sizeof(*link_key_neg_reply));
}

static void bt_hci_cmd_pin_code_reply(void *cp) {
    struct bt_hci_cp_pin_code_reply *pin_code_reply = (struct bt_hci_cp_pin_code_reply *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)pin_code_reply, cp, sizeof(*pin_code_reply));

    bt_hci_cmd(BT_HCI_OP_PIN_CODE_REPLY, sizeof(*pin_code_reply));
}

#if 0
static void bt_hci_cmd_pin_code_neg_reply(void *bdaddr) {
    struct bt_hci_cp_pin_code_neg_reply *pin_code_neg_reply = (struct bt_hci_cp_pin_code_neg_reply *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&pin_code_neg_reply->bdaddr, bdaddr, sizeof(pin_code_neg_reply->bdaddr));

    bt_hci_cmd(BT_HCI_OP_PIN_CODE_NEG_REPLY, sizeof(*pin_code_neg_reply));
}
#endif

static void bt_hci_cmd_auth_requested(void *handle) {
    struct bt_hci_cp_auth_requested *auth_requested = (struct bt_hci_cp_auth_requested *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    auth_requested->handle = *(uint16_t *)handle;

    bt_hci_cmd(BT_HCI_OP_AUTH_REQUESTED, sizeof(*auth_requested));
}

static void bt_hci_cmd_set_conn_encrypt(void *handle) {
    struct bt_hci_cp_set_conn_encrypt *set_conn_encrypt = (struct bt_hci_cp_set_conn_encrypt *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    set_conn_encrypt->handle = *(uint16_t *)handle;
    set_conn_encrypt->encrypt = 0x01;

    bt_hci_cmd(BT_HCI_OP_SET_CONN_ENCRYPT, sizeof(*set_conn_encrypt));
}

static void bt_hci_cmd_remote_name_request(void *bdaddr) {
    struct bt_hci_cp_remote_name_request *remote_name_request = (struct bt_hci_cp_remote_name_request *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&remote_name_request->bdaddr, bdaddr, sizeof(remote_name_request->bdaddr));
    remote_name_request->pscan_rep_mode = 0x01; /* R1 */
    remote_name_request->reserved = 0x00;
    remote_name_request->clock_offset = 0x0000;

    bt_hci_cmd(BT_HCI_OP_REMOTE_NAME_REQUEST, sizeof(*remote_name_request));
}

static void bt_hci_cmd_read_remote_features(void *handle) {
    struct bt_hci_cp_read_remote_features *read_remote_features = (struct bt_hci_cp_read_remote_features *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    read_remote_features->handle = *(uint16_t *)handle;

    bt_hci_cmd(BT_HCI_OP_READ_REMOTE_FEATURES, sizeof(*read_remote_features));
}

static void bt_hci_cmd_read_remote_ext_features(void *handle) {
    struct bt_hci_cp_read_remote_ext_features *read_remote_ext_features = (struct bt_hci_cp_read_remote_ext_features *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    read_remote_ext_features->handle = *(uint16_t *)handle;
    read_remote_ext_features->page = 0x01;

    bt_hci_cmd(BT_HCI_OP_READ_REMOTE_EXT_FEATURES, sizeof(*read_remote_ext_features));
}

static void bt_hci_cmd_io_capability_reply(void *bdaddr) {
    struct bt_hci_cp_io_capability_reply *io_capability_reply = (struct bt_hci_cp_io_capability_reply *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&io_capability_reply->bdaddr, bdaddr, sizeof(io_capability_reply->bdaddr));
    io_capability_reply->capability = 0x03;
    io_capability_reply->oob_data = 0x00;
    io_capability_reply->authentication = 0x00;

    bt_hci_cmd(BT_HCI_OP_IO_CAPABILITY_REPLY, sizeof(*io_capability_reply));
}

static void bt_hci_cmd_user_confirm_reply(void *bdaddr) {
    struct bt_hci_cp_user_confirm_reply *user_confirm_reply = (struct bt_hci_cp_user_confirm_reply *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&user_confirm_reply->bdaddr, bdaddr, sizeof(user_confirm_reply->bdaddr));

    bt_hci_cmd(BT_HCI_OP_USER_CONFIRM_REPLY, sizeof(*user_confirm_reply));
}

#if 0
static void bt_hci_cmd_switch_role(void *cp) {
    struct bt_hci_cp_switch_role *switch_role = (struct bt_hci_cp_switch_role *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)switch_role, cp, sizeof(*switch_role));

    bt_hci_cmd(BT_HCI_OP_SWITCH_ROLE, sizeof(*switch_role));
}

static void bt_hci_cmd_read_link_policy(void *handle) {
    struct bt_hci_cp_read_link_policy *read_link_policy = (struct bt_hci_cp_read_link_policy *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    read_link_policy->handle = *(uint16_t *)handle;

    bt_hci_cmd(BT_HCI_OP_READ_LINK_POLICY, sizeof(*read_link_policy));
}

static void bt_hci_cmd_write_link_policy(void *cp) {
    struct bt_hci_cp_write_link_policy *write_link_policy = (struct bt_hci_cp_write_link_policy *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)write_link_policy, cp, sizeof(*write_link_policy));

    bt_hci_cmd(BT_HCI_OP_WRITE_LINK_POLICY, sizeof(*write_link_policy));
}

static void bt_hci_cmd_read_default_link_policy(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_DEFAULT_LINK_POLICY, 0);
}
#endif

static void bt_hci_cmd_write_default_link_policy(void *cp) {
    struct bt_hci_cp_write_default_link_policy *write_default_link_policy = (struct bt_hci_cp_write_default_link_policy *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_default_link_policy->link_policy = 0x000F;

    bt_hci_cmd(BT_HCI_OP_WRITE_DEFAULT_LINK_POLICY, sizeof(*write_default_link_policy));
}

static void bt_hci_cmd_set_event_mask(void *cp) {
    struct bt_hci_cp_set_event_mask *set_event_mask = (struct bt_hci_cp_set_event_mask *)&bt_hci_pkt_tmp.cp;
    uint8_t events[8] = {0xff, 0xff, 0xfb, 0xff, 0x07, 0xf8, 0xbf, 0x3d};
    printf("# %s\n", __FUNCTION__);

    memcpy(set_event_mask->events, events, sizeof(*set_event_mask));

    bt_hci_cmd(BT_HCI_OP_SET_EVENT_MASK, sizeof(*set_event_mask));
}

static void bt_hci_cmd_reset(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_RESET, 0);
}

static void bt_hci_cmd_set_event_filter(void *cp) {
    struct bt_hci_cp_set_event_filter *set_event_filter = (struct bt_hci_cp_set_event_filter *)&bt_hci_pkt_tmp.cp;
    uint32_t len = 1;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)set_event_filter, cp, sizeof(*set_event_filter));

    if (set_event_filter->filter_type == BT_BREDR_FILTER_TYPE_INQUIRY) {
        len += 1;
    }
    else if (set_event_filter->filter_type == BT_BREDR_FILTER_TYPE_CONN) {
        len += 2;
    }
    if (set_event_filter->condition_type) {
        len += 6;
    }

    bt_hci_cmd(BT_HCI_OP_SET_EVENT_FILTER, len);
}

static void bt_hci_cmd_read_stored_link_key(void *cp) {
    struct bt_hci_cp_read_stored_link_key *read_stored_link_key = (struct bt_hci_cp_read_stored_link_key *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memset((void *)&read_stored_link_key->bdaddr, 0, sizeof(read_stored_link_key->bdaddr));
    read_stored_link_key->read_all_flag = 0x01;

    bt_hci_cmd(BT_HCI_OP_READ_STORED_LINK_KEY, sizeof(*read_stored_link_key));
}

static void bt_hci_cmd_delete_stored_link_key(void *cp) {
    struct bt_hci_cp_delete_stored_link_key *delete_stored_link_key = (struct bt_hci_cp_delete_stored_link_key *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memset((void *)&delete_stored_link_key->bdaddr, 0, sizeof(delete_stored_link_key->bdaddr));
    delete_stored_link_key->delete_all_flag = 0x01;

    bt_hci_cmd(BT_HCI_OP_DELETE_STORED_LINK_KEY, sizeof(*delete_stored_link_key));
}

static void bt_hci_cmd_write_local_name(void *cp) {
    struct bt_hci_cp_write_local_name *write_local_name = (struct bt_hci_cp_write_local_name *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memset((void *)write_local_name, 0, sizeof(*write_local_name));
    snprintf((char *)write_local_name->local_name, sizeof(write_local_name->local_name), "BlueRetro Adapter");

    bt_hci_cmd(BT_HCI_OP_WRITE_LOCAL_NAME, sizeof(*write_local_name));
}

static void bt_hci_cmd_read_local_name(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_NAME, 0);
}

static void bt_hci_cmd_write_conn_accept_timeout(void *cp) {
    struct bt_hci_cp_write_conn_accept_timeout *write_conn_accept_timeout = (struct bt_hci_cp_write_conn_accept_timeout *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_conn_accept_timeout->conn_accept_timeout = 0x7d00;

    bt_hci_cmd(BT_HCI_OP_WRITE_CONN_ACCEPT_TIMEOUT, sizeof(*write_conn_accept_timeout));
}

static void bt_hci_cmd_write_page_scan_timeout(void *cp) {
    struct bt_hci_cp_write_page_scan_timeout *write_page_scan_timeout = (struct bt_hci_cp_write_page_scan_timeout *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_page_scan_timeout->timeout = 0x2000;

    bt_hci_cmd(BT_HCI_OP_WRITE_PAGE_TIMEOUT, sizeof(*write_page_scan_timeout));
}

static void bt_hci_cmd_write_scan_enable(void *cp) {
    struct bt_hci_cp_write_scan_enable *write_scan_enable = (struct bt_hci_cp_write_scan_enable *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_scan_enable->scan_enable = BT_BREDR_SCAN_PAGE;

    bt_hci_cmd(BT_HCI_OP_WRITE_SCAN_ENABLE, sizeof(*write_scan_enable));
}

static void bt_hci_cmd_write_page_scan_activity(void *cp) {
    struct bt_hci_cp_write_page_scan_activity *write_page_scan_activity = (struct bt_hci_cp_write_page_scan_activity *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_page_scan_activity->interval = 0x50;
    write_page_scan_activity->window = 0x12;

    bt_hci_cmd(BT_HCI_OP_WRITE_PAGE_SCAN_ACTIVITY, sizeof(*write_page_scan_activity));
}

static void bt_hci_cmd_write_inquiry_scan_activity(void *cp) {
    struct bt_hci_cp_write_inquiry_scan_activity *write_inquiry_scan_activity = (struct bt_hci_cp_write_inquiry_scan_activity *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_inquiry_scan_activity->interval = 0x50;
    write_inquiry_scan_activity->window = 0x12;

    bt_hci_cmd(BT_HCI_OP_WRITE_INQUIRY_SCAN_ACTIVITY, sizeof(*write_inquiry_scan_activity));
}

static void bt_hci_cmd_write_auth_enable(void *cp) {
    struct bt_hci_cp_write_auth_enable *write_auth_enable = (struct bt_hci_cp_write_auth_enable *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_auth_enable->auth_enable = 0x00;

    bt_hci_cmd(BT_HCI_OP_WRITE_AUTH_ENABLE, sizeof(*write_auth_enable));
}

static void bt_hci_cmd_read_page_scan_activity(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_PAGE_SCAN_ACTIVITY, 0);
}

static void bt_hci_cmd_read_class_of_device(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_CLASS_OF_DEVICE, 0);
}

static void bt_hci_cmd_write_class_of_device(void *cp) {
    struct bt_hci_cp_write_class_of_device *write_class_of_device = (struct bt_hci_cp_write_class_of_device *)&bt_hci_pkt_tmp.cp;
    uint8_t local_class[3] = {0x0c, 0x01, 0x1c}; /* Laptop */
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)write_class_of_device->dev_class, (void *)local_class, sizeof(local_class));

    bt_hci_cmd(BT_HCI_OP_WRITE_CLASS_OF_DEVICE, sizeof(*write_class_of_device));
}

static void bt_hci_cmd_read_voice_setting(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_VOICE_SETTING, 0);
}

static void bt_hci_cmd_write_hold_mode_act(void *cp) {
    struct bt_hci_cp_write_hold_mode_act *write_hold_mode_act = (struct bt_hci_cp_write_hold_mode_act *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_hold_mode_act->activity = 0x00;

    bt_hci_cmd(BT_HCI_OP_WRITE_HOLD_MODE_ACT, sizeof(*write_hold_mode_act));
}

static void bt_hci_cmd_read_num_supported_iac(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_NUM_SUPPORTED_IAC, 0);
}

static void bt_hci_cmd_read_current_iac_lap(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_CURRENT_IAC_LAP, 0);
}

static void bt_hci_cmd_write_inquiry_mode(void *cp) {
    struct bt_hci_cp_write_inquiry_mode *write_inquiry_mode = (struct bt_hci_cp_write_inquiry_mode *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_inquiry_mode->mode = 0x02;

    bt_hci_cmd(BT_HCI_OP_WRITE_INQUIRY_MODE, sizeof(*write_inquiry_mode));
}

static void bt_hci_cmd_read_page_scan_type(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_PAGE_SCAN_TYPE, 0);
}

static void bt_hci_cmd_write_page_scan_type(void *cp) {
    struct bt_hci_cp_write_page_scan_type *write_page_scan_type = (struct bt_hci_cp_write_page_scan_type *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_page_scan_type->type = 0x01;

    bt_hci_cmd(BT_HCI_OP_WRITE_PAGE_SCAN_TYPE, sizeof(*write_page_scan_type));
}

static void bt_hci_cmd_write_ssp_mode(void *cp) {
    struct bt_hci_cp_write_ssp_mode *write_ssp_mode = (struct bt_hci_cp_write_ssp_mode *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_ssp_mode->mode = 0x01;

    bt_hci_cmd(BT_HCI_OP_WRITE_SSP_MODE, sizeof(*write_ssp_mode));
}

static void bt_hci_cmd_read_inquiry_rsp_tx_pwr_lvl(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_INQUIRY_RSP_TX_PWR_LVL, 0);
}

static void bt_hci_cmd_write_le_host_supp(void *cp) {
    struct bt_hci_cp_write_le_host_supp *write_le_host_supp = (struct bt_hci_cp_write_le_host_supp *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_le_host_supp->le = 0x00;
    write_le_host_supp->simul = 0x00;

    bt_hci_cmd(BT_HCI_OP_LE_WRITE_LE_HOST_SUPP, sizeof(*write_le_host_supp));
}

static void bt_hci_cmd_read_local_version_info(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_VERSION_INFO, 0);
}

static void bt_hci_cmd_read_supported_commands(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_SUPPORTED_COMMANDS, 0);
}

static void bt_hci_cmd_read_local_features(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_FEATURES, 0);
}

static void bt_hci_cmd_read_local_ext_features(void *cp) {
    struct bt_hci_cp_read_local_ext_features *read_local_ext_features = (struct bt_hci_cp_read_local_ext_features *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    read_local_ext_features->page = 0x01;

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_EXT_FEATURES, sizeof(*read_local_ext_features));
}

static void bt_hci_cmd_read_buffer_size(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_BUFFER_SIZE, 0);
}

static void bt_hci_cmd_read_bd_addr(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_BD_ADDR, 0);
}

#if 0
static void bt_hci_cmd_read_data_block_size(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_DATA_BLOCK_SIZE, 0);
}

static void bt_hci_cmd_read_local_codecs(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_CODECS, 0);
}

static void bt_hci_cmd_read_local_sp_options(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_SP_OPTIONS, 0);
}
#endif

void bt_hci_init(void) {
    bt_config_state = 0;
    bt_hci_q_conf(0);
}

void bt_hci_evt_hdlr(struct bt_hci_pkt *bt_hci_evt_pkt) {
    struct bt_dev *device = NULL;

    switch (bt_hci_evt_pkt->evt_hdr.evt) {
        case BT_HCI_EVT_INQUIRY_COMPLETE:
            printf("# BT_HCI_EVT_INQUIRY_COMPLETE\n");
            bt_nb_inquiry++;
            if (bt_host_get_active_dev(&device) > -1 && bt_nb_inquiry > BT_INQUIRY_MAX) {
                bt_hci_cmd_exit_periodic_inquiry(NULL);
            }
            break;
        case BT_HCI_EVT_INQUIRY_RESULT:
        case BT_HCI_EVT_INQUIRY_RESULT_WITH_RSSI:
        case BT_HCI_EVT_EXTENDED_INQUIRY_RESULT:
        {
            struct bt_hci_evt_inquiry_result *inquiry_result = (struct bt_hci_evt_inquiry_result *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_INQUIRY_RESULT\n");
            printf("# Number of responce: %d\n", inquiry_result->num_reports);
            for (uint8_t i = 1; i <= inquiry_result->num_reports; i++) {
                bt_host_get_dev_from_bdaddr((bt_addr_t *)((uint8_t *)&inquiry_result + 1 + 6*(i - 1)), &device);
                if (device == NULL) {
                    int32_t bt_dev_id = bt_host_get_new_dev(&device);
                    if (device) {
                        memcpy(device->remote_bdaddr, (uint8_t *)inquiry_result + 1 + 6*(i - 1), sizeof(device->remote_bdaddr));
                        device->id = bt_dev_id;
                        device->type = bt_hid_minor_class_to_type(((uint8_t *)inquiry_result + 1 + 9*i)[0]);
                        bt_l2cap_init_dev_scid(device);
                        atomic_set_bit(&device->flags, BT_DEV_DEVICE_FOUND);
                        bt_hci_cmd_connect(device->remote_bdaddr);
                        printf("# Inquiry dev: %d type: %d bdaddr: %02X:%02X:%02X:%02X:%02X:%02X\n", device->id, device->type,
                            device->remote_bdaddr[5], device->remote_bdaddr[4], device->remote_bdaddr[3],
                            device->remote_bdaddr[2], device->remote_bdaddr[1], device->remote_bdaddr[0]);
                    }
                }
                break; /* Only support one result for now */
            }
            break;
        }
        case BT_HCI_EVT_CONN_COMPLETE:
        {
            struct bt_hci_evt_conn_complete *conn_complete = (struct bt_hci_evt_conn_complete *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_CONN_COMPLETE\n");
            bt_host_get_dev_from_bdaddr(&conn_complete->bdaddr, &device);
            if (device) {
                if (conn_complete->status) {
                    device->pkt_retry++;
                    printf("# dev: %d error: 0x%02X\n", device->id, conn_complete->status);
                    if (!atomic_test_bit(&device->flags, BT_DEV_PAGE) && device->pkt_retry < BT_MAX_RETRY) {
                        bt_hci_cmd_connect(device->remote_bdaddr);
                    }
                    else {
                        bt_host_reset_dev(device);
                        if (bt_host_get_active_dev(&device) == BT_NONE) {
                            bt_hci_cmd_periodic_inquiry(NULL);
                        }
                    }
                }
                else {
                    device->acl_handle = conn_complete->handle;
                    device->pkt_retry = 0;
                    printf("# dev: %d acl_handle: 0x%04X\n", device->id, device->acl_handle);
                    bt_hci_cmd_remote_name_request(device->remote_bdaddr);
                    bt_hci_cmd_read_remote_features(&device->acl_handle);
                    if (!atomic_test_bit(&device->flags, BT_DEV_PAGE)) {
                        bt_hci_cmd_auth_requested(&device->acl_handle);
                    }
                }
            }
            else {
                printf("# dev NULL!\n");
            }
            break;
        }
        case BT_HCI_EVT_CONN_REQUEST:
        {
            struct bt_hci_evt_conn_request *conn_request = (struct bt_hci_evt_conn_request *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_CONN_REQUEST\n");
            bt_host_get_dev_from_bdaddr(&conn_request->bdaddr, &device);
            if (device == NULL) {
                int32_t bt_dev_id = bt_host_get_new_dev(&device);
                if (device) {
                    memcpy(device->remote_bdaddr, (void *)&conn_request->bdaddr, sizeof(device->remote_bdaddr));
                    device->id = bt_dev_id;
                    device->type = bt_hid_minor_class_to_type(conn_request->dev_class[0]);
                    bt_l2cap_init_dev_scid(device);
                    atomic_set_bit(&device->flags, BT_DEV_DEVICE_FOUND);
                    atomic_set_bit(&device->flags, BT_DEV_PAGE);
                    bt_hci_cmd_accept_conn_req(device->remote_bdaddr);
                    printf("# Page dev: %d type: %d bdaddr: %02X:%02X:%02X:%02X:%02X:%02X\n", device->id, device->type,
                        device->remote_bdaddr[5], device->remote_bdaddr[4], device->remote_bdaddr[3],
                        device->remote_bdaddr[2], device->remote_bdaddr[1], device->remote_bdaddr[0]);
                }
            }
            break;
        }
        case BT_HCI_EVT_DISCONN_COMPLETE:
        {
            struct bt_hci_evt_disconn_complete *disconn_complete = (struct bt_hci_evt_disconn_complete *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_DISCONN_COMPLETE\n");
            bt_host_get_dev_from_handle(disconn_complete->handle, &device);
            bt_host_reset_dev(device);
            if (bt_host_get_active_dev(&device) == BT_NONE) {
                bt_hci_cmd_periodic_inquiry(NULL);
            }
            break;
        }
        case BT_HCI_EVT_AUTH_COMPLETE:
        {
            struct bt_hci_evt_auth_complete *auth_complete = (struct bt_hci_evt_auth_complete *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_AUTH_COMPLETE\n");
            bt_host_get_dev_from_handle(auth_complete->handle, &device);
            if (device) {
                if (auth_complete->status) {
                    printf("# dev: %d error: 0x%02X\n", device->id, auth_complete->status);
                }
                else {
                    printf("# dev: %d Pairing done\n", device->id);
                    if (!atomic_test_bit(&device->flags, BT_DEV_PAGE)) {
                        if (atomic_test_bit(&device->flags, BT_DEV_ENCRYPTION)) {
                            bt_hci_cmd_set_conn_encrypt(&device->acl_handle);
                        }
                        bt_l2cap_cmd_hid_ctrl_conn_req(device);
                    }
                }
            }
            else {
                printf("# dev NULL!\n");
            }
            break;
        }
        case BT_HCI_EVT_REMOTE_NAME_REQ_COMPLETE:
        {
            struct bt_hci_evt_remote_name_req_complete *remote_name_req_complete = (struct bt_hci_evt_remote_name_req_complete *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_REMOTE_NAME_REQ_COMPLETE:\n");
            bt_host_get_dev_from_bdaddr(&remote_name_req_complete->bdaddr, &device);
            if (device) {
                if (remote_name_req_complete->status) {
                    device->pkt_retry++;
                    printf("# dev: %d error: 0x%02X\n", device->id, remote_name_req_complete->status);
                    if (device->pkt_retry < BT_MAX_RETRY) {
                        bt_hci_cmd_remote_name_request(device->remote_bdaddr);
                    }
                    else {
                        bt_host_reset_dev(device);
                        if (bt_host_get_active_dev(&device) == BT_NONE) {
                            bt_hci_cmd_periodic_inquiry(NULL);
                        }
                    }
                }
                else {
                    int8_t type = bt_hci_get_type_from_name(remote_name_req_complete->name);
                    if (type > BT_NONE) {
                        device->type = bt_hci_get_type_from_name(remote_name_req_complete->name);
                    }
                    printf("# dev: %d type: %d %s\n", device->id, device->type, remote_name_req_complete->name);
                }
            }
            else {
                printf("# dev NULL!\n");
            }
            break;
        }
        case BT_HCI_EVT_ENCRYPT_CHANGE:
        {
            struct bt_hci_evt_encrypt_change *encrypt_change = (struct bt_hci_evt_encrypt_change *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_ENCRYPT_CHANGE\n");
            bt_host_get_dev_from_handle(encrypt_change->handle, &device);
            if (device) {
                if (encrypt_change->status) {
                    printf("# dev: %d error: 0x%02X\n", device->id, encrypt_change->status);
                }
            }
            else {
                printf("# dev NULL!\n");
            }
            break;
        }
        case BT_HCI_EVT_REMOTE_FEATURES:
        {
            struct bt_hci_evt_remote_features *remote_features = (struct bt_hci_evt_remote_features *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_REMOTE_FEATURES\n");
            bt_host_get_dev_from_handle(remote_features->handle, &device);
            if (device) {
                if (remote_features->status) {
                    printf("# dev: %d error: 0x%02X\n", device->id, remote_features->status);
                }
                else {
                    if (remote_features->features[8] & 0x80) {
                        bt_hci_cmd_read_remote_ext_features(&device->acl_handle);
                    }
                    if (remote_features->features[0] & 0x04) {
                        atomic_set_bit(&device->flags, BT_DEV_ENCRYPTION);
                    }
                }
                bt_l2cap_cmd_sdp_conn_req(device);
            }
            else {
                printf("# dev NULL!\n");
            }
            break;
        }
        case BT_HCI_EVT_CMD_COMPLETE:
        {
            struct bt_hci_evt_cmd_complete *cmd_complete = (struct bt_hci_evt_cmd_complete *)bt_hci_evt_pkt->evt_data;
            uint8_t status = bt_hci_evt_pkt->evt_data[sizeof(*cmd_complete)];
            printf("# BT_HCI_EVT_CMD_COMPLETE\n");
            if (status != BT_HCI_ERR_SUCCESS && status != BT_HCI_ERR_UNKNOWN_CMD) {
                printf("# opcode: 0x%04X error: 0x%02X retry: %d\n", cmd_complete->opcode, status, bt_hci_pkt_retry);
                switch (cmd_complete->opcode) {
                    case BT_HCI_OP_READ_BD_ADDR:
                    case BT_HCI_OP_RESET:
                    case BT_HCI_OP_READ_LOCAL_FEATURES:
                    case BT_HCI_OP_READ_LOCAL_VERSION_INFO:
                    case BT_HCI_OP_READ_BUFFER_SIZE:
                    case BT_HCI_OP_READ_CLASS_OF_DEVICE:
                    case BT_HCI_OP_READ_LOCAL_NAME:
                    case BT_HCI_OP_READ_VOICE_SETTING:
                    case BT_HCI_OP_READ_NUM_SUPPORTED_IAC:
                    case BT_HCI_OP_READ_CURRENT_IAC_LAP:
                    case BT_HCI_OP_SET_EVENT_FILTER:
                    case BT_HCI_OP_WRITE_CONN_ACCEPT_TIMEOUT:
                    case BT_HCI_OP_READ_SUPPORTED_COMMANDS:
                    case BT_HCI_OP_WRITE_SSP_MODE:
                    case BT_HCI_OP_WRITE_INQUIRY_MODE:
                    case BT_HCI_OP_READ_INQUIRY_RSP_TX_PWR_LVL:
                    case BT_HCI_OP_READ_LOCAL_EXT_FEATURES:
                    case BT_HCI_OP_READ_STORED_LINK_KEY:
                    case BT_HCI_OP_READ_PAGE_SCAN_ACTIVITY:
                    case BT_HCI_OP_READ_PAGE_SCAN_TYPE:
                    case BT_HCI_OP_LE_WRITE_LE_HOST_SUPP:
                    case BT_HCI_OP_DELETE_STORED_LINK_KEY:
                    case BT_HCI_OP_WRITE_CLASS_OF_DEVICE:
                    case BT_HCI_OP_WRITE_LOCAL_NAME:
                    case BT_HCI_OP_WRITE_AUTH_ENABLE:
                    case BT_HCI_OP_SET_EVENT_MASK:
                    case BT_HCI_OP_WRITE_PAGE_SCAN_ACTIVITY:
                    case BT_HCI_OP_WRITE_INQUIRY_SCAN_ACTIVITY:
                    case BT_HCI_OP_WRITE_PAGE_SCAN_TYPE:
                    case BT_HCI_OP_WRITE_PAGE_TIMEOUT:
                    case BT_HCI_OP_WRITE_HOLD_MODE_ACT:
                    case BT_HCI_OP_WRITE_SCAN_ENABLE:
                    case BT_HCI_OP_WRITE_DEFAULT_LINK_POLICY:
                        bt_hci_pkt_retry++;
                        if (bt_hci_pkt_retry > BT_MAX_RETRY) {
                            bt_hci_pkt_retry = 0;
                            bt_hci_init();
                        }
                        else {
                            bt_hci_q_conf(0);
                        }
                        break;
                }
            }
            else {
                switch (cmd_complete->opcode) {
                    case BT_HCI_OP_READ_BD_ADDR:
                    {
                        struct bt_hci_rp_read_bd_addr *read_bd_addr = (struct bt_hci_rp_read_bd_addr *)&bt_hci_evt_pkt->evt_data[sizeof(*cmd_complete)];
                        memcpy((void *)local_bdaddr, (void *)&read_bd_addr->bdaddr, sizeof(local_bdaddr));
                        printf("# local_bdaddr: %02X:%02X:%02X:%02X:%02X:%02X\n",
                            local_bdaddr[5], local_bdaddr[4], local_bdaddr[3],
                            local_bdaddr[2], local_bdaddr[1], local_bdaddr[0]);
                    }
                        /* Fall-through */
                    case BT_HCI_OP_RESET:
                    case BT_HCI_OP_READ_LOCAL_FEATURES:
                    case BT_HCI_OP_READ_LOCAL_VERSION_INFO:
                    case BT_HCI_OP_READ_BUFFER_SIZE:
                    case BT_HCI_OP_READ_CLASS_OF_DEVICE:
                    case BT_HCI_OP_READ_LOCAL_NAME:
                    case BT_HCI_OP_READ_VOICE_SETTING:
                    case BT_HCI_OP_READ_NUM_SUPPORTED_IAC:
                    case BT_HCI_OP_READ_CURRENT_IAC_LAP:
                    case BT_HCI_OP_SET_EVENT_FILTER:
                    case BT_HCI_OP_WRITE_CONN_ACCEPT_TIMEOUT:
                    case BT_HCI_OP_READ_SUPPORTED_COMMANDS:
                    case BT_HCI_OP_WRITE_SSP_MODE:
                    case BT_HCI_OP_WRITE_INQUIRY_MODE:
                    case BT_HCI_OP_READ_INQUIRY_RSP_TX_PWR_LVL:
                    case BT_HCI_OP_READ_LOCAL_EXT_FEATURES:
                    case BT_HCI_OP_READ_STORED_LINK_KEY:
                    case BT_HCI_OP_READ_PAGE_SCAN_ACTIVITY:
                    case BT_HCI_OP_READ_PAGE_SCAN_TYPE:
                    case BT_HCI_OP_LE_WRITE_LE_HOST_SUPP:
                    case BT_HCI_OP_DELETE_STORED_LINK_KEY:
                    case BT_HCI_OP_WRITE_CLASS_OF_DEVICE:
                    case BT_HCI_OP_WRITE_LOCAL_NAME:
                    case BT_HCI_OP_WRITE_AUTH_ENABLE:
                    case BT_HCI_OP_SET_EVENT_MASK:
                    case BT_HCI_OP_WRITE_PAGE_SCAN_ACTIVITY:
                    case BT_HCI_OP_WRITE_INQUIRY_SCAN_ACTIVITY:
                    case BT_HCI_OP_WRITE_PAGE_SCAN_TYPE:
                    case BT_HCI_OP_WRITE_PAGE_TIMEOUT:
                    case BT_HCI_OP_WRITE_HOLD_MODE_ACT:
                    case BT_HCI_OP_WRITE_SCAN_ENABLE:
                    case BT_HCI_OP_WRITE_DEFAULT_LINK_POLICY:
                        bt_hci_pkt_retry = 0;
                        bt_hci_q_conf(1);
                        break;
                }
            }
            break;
        }
        case BT_HCI_EVT_CMD_STATUS:
        {
            struct bt_hci_evt_cmd_status *cmd_status = (struct bt_hci_evt_cmd_status *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_CMD_STATUS\n");
            if (cmd_status->status) {
                printf("# opcode: 0x%04X error: 0x%02X\n", cmd_status->opcode, cmd_status->status);
            }
            break;
        }
        case BT_HCI_EVT_ROLE_CHANGE:
        {
            struct bt_hci_evt_role_change *role_change = (struct bt_hci_evt_role_change *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_ROLE_CHANGE\n");
            bt_host_get_dev_from_bdaddr(&role_change->bdaddr, &device);
            if (device) {
                if (role_change->status) {
                    atomic_set_bit(&device->flags, BT_DEV_ROLE_SW_FAIL);
                }
                else {
                    atomic_clear_bit(&device->flags, BT_DEV_ROLE_SW_FAIL);
                }
            }
            else {
                printf("# dev NULL!\n");
            }
            break;
        }
        case BT_HCI_EVT_PIN_CODE_REQ:
        {
            struct bt_hci_evt_pin_code_req *pin_code_req = (struct bt_hci_evt_pin_code_req *)bt_hci_evt_pkt->evt_data;
            struct bt_hci_cp_pin_code_reply pin_code_reply = {0};
            printf("# BT_HCI_EVT_PIN_CODE_REQ\n");
            bt_host_get_dev_from_bdaddr(&pin_code_req->bdaddr, &device);
            memcpy((void *)&pin_code_reply.bdaddr, device->remote_bdaddr, sizeof(pin_code_reply.bdaddr));
            if (bt_dev_is_wii(device->type)) {
                memcpy(pin_code_reply.pin_code, local_bdaddr, sizeof(local_bdaddr));
                pin_code_reply.pin_len = sizeof(local_bdaddr);
            }
            else {
                memcpy(pin_code_reply.pin_code, bt_default_pin[0], strlen(bt_default_pin[0]));
                pin_code_reply.pin_len = strlen(bt_default_pin[0]);
            }
            bt_hci_cmd_pin_code_reply(&pin_code_reply);
            break;
        }
        case BT_HCI_EVT_LINK_KEY_REQ:
        {
            struct bt_hci_evt_link_key_req *link_key_req = (struct bt_hci_evt_link_key_req *)bt_hci_evt_pkt->evt_data;
            struct bt_hci_cp_link_key_reply link_key_reply;
            printf("# BT_HCI_EVT_LINK_KEY_REQ\n");
            bt_host_get_dev_from_bdaddr(&link_key_req->bdaddr, &device);
            memcpy((void *)&link_key_reply.bdaddr, (void *)&link_key_req->bdaddr, sizeof(link_key_reply.bdaddr));
            if (atomic_test_bit(&device->flags, BT_DEV_PAGE) && bt_host_load_link_key(&link_key_reply) == 0) {
                bt_hci_cmd_link_key_reply((void *)&link_key_reply);
            }
            else {
                bt_hci_cmd_link_key_neg_reply((void *)device->remote_bdaddr);
            }
            break;
        }
        case BT_HCI_EVT_LINK_KEY_NOTIFY:
        {
            struct bt_hci_evt_link_key_notify *link_key_notify = (struct bt_hci_evt_link_key_notify *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_LINK_KEY_NOTIFY\n");
            bt_host_get_dev_from_bdaddr(&link_key_notify->bdaddr, &device);
            bt_host_store_link_key(link_key_notify);
            break;
        }
        case BT_HCI_EVT_REMOTE_EXT_FEATURES:
        {
            struct bt_hci_evt_remote_ext_features *remote_ext_features = (struct bt_hci_evt_remote_ext_features *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_REMOTE_EXT_FEATURES\n");
            bt_host_get_dev_from_handle(remote_ext_features->handle, &device);
            if (device) {
                if (remote_ext_features->status) {
                    printf("# dev: %d error: 0x%02X\n", device->id, remote_ext_features->status);
                }
            }
            else {
                printf("# dev NULL!\n");
            }
            break;
        }
        case BT_HCI_EVT_IO_CAPA_REQ:
        {
            struct bt_hci_evt_io_capa_req *io_capa_req = (struct bt_hci_evt_io_capa_req *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_IO_CAPA_REQ\n");
            bt_host_get_dev_from_bdaddr(&io_capa_req->bdaddr, &device);
            if (device) {
                bt_hci_cmd_io_capability_reply((void *)device->remote_bdaddr);
            }
            break;
        }
        case BT_HCI_EVT_USER_CONFIRM_REQ:
        {
            struct bt_hci_evt_user_confirm_req *user_confirm_req = (struct bt_hci_evt_user_confirm_req *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_USER_CONFIRM_REQ\n");
            bt_host_get_dev_from_bdaddr(&user_confirm_req->bdaddr, &device);
            if (device) {
                bt_hci_cmd_user_confirm_reply((void *)device->remote_bdaddr);
            }
            break;
        }
    }
}
