#include <stdio.h>
#include "zephyr/hci.h"
#include "bt_host.h"

struct bt_hci_tx_packet {
    struct bt_hci_h4_hdr h4_hdr;
    struct bt_hci_cmd_hdr cmd_hdr;
    uint8_t cp[512];
} __packed;

static struct bt_hci_tx_packet bt_hci_tx_pkt_tmp;

static void bt_hci_cmd(uint16_t opcode, uint32_t cp_len) {
    uint32_t packet_len = BT_HCI_H4_HDR_SIZE + BT_HCI_CMD_HDR_SIZE + cp_len;

    bt_hci_tx_pkt_tmp.h4_hdr.type = BT_HCI_H4_TYPE_CMD;

    bt_hci_tx_pkt_tmp.cmd_hdr.opcode = opcode;
    bt_hci_tx_pkt_tmp.cmd_hdr.param_len = cp_len;

    bt_host_txq_add((uint8_t *)&bt_hci_tx_pkt_tmp, packet_len);
}

void bt_hci_cmd_inquiry(void *cp) {
    struct bt_hci_cp_inquiry *inquiry = (struct bt_hci_cp_inquiry *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    inquiry->lap[0] = 0x33;
    inquiry->lap[1] = 0x8B;
    inquiry->lap[2] = 0x9E;
    inquiry->length = 0x20; /* 0x02 * 1.28 s = 2.56 s */
    inquiry->num_rsp = 0xFF;

    bt_hci_cmd(BT_HCI_OP_INQUIRY, sizeof(*inquiry));
}

void bt_hci_cmd_inquiry_cancel(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_INQUIRY_CANCEL, 0);
}

void bt_hci_cmd_connect(bt_addr_t *bdaddr) {
    struct bt_hci_cp_connect *connect = (struct bt_hci_cp_connect *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&connect->bdaddr, (void *)bdaddr, sizeof(*bdaddr));
    connect->packet_type = 0xCC18; /* DH5, DM5, DH3, DM3, DH1 & DM1 */
    connect->pscan_rep_mode = 0x00; /* R1 */
    connect->reserved = 0x00;
    connect->clock_offset = 0x0000;
    connect->allow_role_switch = 0x01;

    bt_hci_cmd(BT_HCI_OP_CONNECT, sizeof(*connect));
}

void bt_hci_cmd_accept_conn_req(bt_addr_t *bdaddr) {
    struct bt_hci_cp_accept_conn_req *accept_conn_req = (struct bt_hci_cp_accept_conn_req *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&accept_conn_req->bdaddr, (void* )bdaddr, sizeof(*bdaddr));
    accept_conn_req->role = 0x00; /* Become master */

    bt_hci_cmd(BT_HCI_OP_ACCEPT_CONN_REQ, sizeof(*accept_conn_req));
}

void bt_hci_cmd_link_key_neg_reply(bt_addr_t *bdaddr) {
    struct bt_hci_cp_link_key_neg_reply *link_key_neg_reply = (struct bt_hci_cp_link_key_neg_reply *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&link_key_neg_reply->bdaddr, (void *)bdaddr, sizeof(*bdaddr));

    bt_hci_cmd(BT_HCI_OP_LINK_KEY_NEG_REPLY, sizeof(*link_key_neg_reply));
}

void bt_hci_cmd_pin_code_reply(bt_addr_t bdaddr, uint8_t pin_len, uint8_t *pin_code) {
    struct bt_hci_cp_pin_code_reply *pin_code_reply = (struct bt_hci_cp_pin_code_reply *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy(pin_code_reply->bdaddr.val, bdaddr.val, sizeof(bdaddr));
    pin_code_reply->pin_len = pin_len;
    memcpy(pin_code_reply->pin_code, pin_code, pin_len);

    bt_hci_cmd(BT_HCI_OP_PIN_CODE_REPLY, sizeof(*pin_code_reply));
}

void bt_hci_cmd_pin_code_neg_reply(bt_addr_t bdaddr) {
    struct bt_hci_cp_pin_code_neg_reply *pin_code_neg_reply = (struct bt_hci_cp_pin_code_neg_reply *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy(pin_code_neg_reply->bdaddr.val, bdaddr.val, sizeof(bdaddr));

    bt_hci_cmd(BT_HCI_OP_PIN_CODE_NEG_REPLY, sizeof(*pin_code_neg_reply));
}

void bt_hci_cmd_auth_requested(uint16_t handle) {
    struct bt_hci_cp_auth_requested *auth_requested = (struct bt_hci_cp_auth_requested *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    auth_requested->handle = handle;

    bt_hci_cmd(BT_HCI_OP_AUTH_REQUESTED, sizeof(*auth_requested));
}

void bt_hci_cmd_set_conn_encrypt(uint16_t handle) {
    struct bt_hci_cp_set_conn_encrypt *set_conn_encrypt = (struct bt_hci_cp_set_conn_encrypt *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    set_conn_encrypt->handle = handle;
    set_conn_encrypt->encrypt = 0x01;

    bt_hci_cmd(BT_HCI_OP_SET_CONN_ENCRYPT, sizeof(*set_conn_encrypt));
}

void bt_hci_cmd_remote_name_request(bt_addr_t bdaddr) {
    struct bt_hci_cp_remote_name_request *remote_name_request = (struct bt_hci_cp_remote_name_request *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy(remote_name_request->bdaddr.val, bdaddr.val, sizeof(bdaddr));
    remote_name_request->pscan_rep_mode = 0x01; /* R1 */
    remote_name_request->reserved = 0x00;
    remote_name_request->clock_offset = 0x0000;

    bt_hci_cmd(BT_HCI_OP_REMOTE_NAME_REQUEST, sizeof(*remote_name_request));
}

void bt_hci_cmd_read_remote_features(uint16_t handle) {
    struct bt_hci_cp_read_remote_features *read_remote_features = (struct bt_hci_cp_read_remote_features *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    read_remote_features->handle = handle;

    bt_hci_cmd(BT_HCI_OP_READ_REMOTE_FEATURES, sizeof(*read_remote_features));
}

void bt_hci_cmd_read_remote_ext_features(uint16_t handle) {
    struct bt_hci_cp_read_remote_ext_features *read_remote_ext_features = (struct bt_hci_cp_read_remote_ext_features *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    read_remote_ext_features->handle = handle;
    read_remote_ext_features->page = 0x01;

    bt_hci_cmd(BT_HCI_OP_READ_REMOTE_EXT_FEATURES, sizeof(*read_remote_ext_features));
}

void bt_hci_cmd_io_capability_reply(bt_addr_t *bdaddr) {
    struct bt_hci_cp_io_capability_reply *io_capability_reply = (struct bt_hci_cp_io_capability_reply *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&io_capability_reply->bdaddr, (void* )bdaddr, sizeof(*bdaddr));
    io_capability_reply->capability = 0x03;
    io_capability_reply->oob_data = 0x00;
    io_capability_reply->authentication = 0x00;

    bt_hci_cmd(BT_HCI_OP_IO_CAPABILITY_REPLY, sizeof(*io_capability_reply));
}

void bt_hci_cmd_user_confirm_reply(bt_addr_t *bdaddr) {
    struct bt_hci_cp_user_confirm_reply *user_confirm_reply = (struct bt_hci_cp_user_confirm_reply *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&user_confirm_reply->bdaddr, (void* )bdaddr, sizeof(*bdaddr));

    bt_hci_cmd(BT_HCI_OP_USER_CONFIRM_REPLY, sizeof(*user_confirm_reply));
}

void bt_hci_cmd_switch_role(bt_addr_t *bdaddr, uint8_t role) {
    struct bt_hci_cp_switch_role *switch_role = (struct bt_hci_cp_switch_role *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&switch_role->bdaddr, (void* )bdaddr, sizeof(*bdaddr));
    switch_role->role = role;

    bt_hci_cmd(BT_HCI_OP_SWITCH_ROLE, sizeof(*switch_role));
}

void bt_hci_cmd_read_link_policy(uint16_t handle) {
    struct bt_hci_cp_read_link_policy *read_link_policy = (struct bt_hci_cp_read_link_policy *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    read_link_policy->handle = handle;

    bt_hci_cmd(BT_HCI_OP_READ_LINK_POLICY, sizeof(*read_link_policy));
}

void bt_hci_cmd_write_link_policy(uint16_t handle, uint16_t link_policy) {
    struct bt_hci_cp_write_link_policy *write_link_policy = (struct bt_hci_cp_write_link_policy *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_link_policy->handle = handle;
    write_link_policy->link_policy = link_policy;

    bt_hci_cmd(BT_HCI_OP_WRITE_LINK_POLICY, sizeof(*write_link_policy));
}

void bt_hci_cmd_read_default_link_policy(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_DEFAULT_LINK_POLICY, 0);
}

void bt_hci_cmd_write_default_link_policy(void *cp) {
    struct bt_hci_cp_write_default_link_policy *write_default_link_policy = (struct bt_hci_cp_write_default_link_policy *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_default_link_policy->link_policy = 0x000F;

    bt_hci_cmd(BT_HCI_OP_WRITE_DEFAULT_LINK_POLICY, sizeof(*write_default_link_policy));
}

void bt_hci_cmd_set_event_mask(void *cp) {
    struct bt_hci_cp_set_event_mask *set_event_mask = (struct bt_hci_cp_set_event_mask *)&bt_hci_tx_pkt_tmp.cp;
    uint8_t events[8] = {0xff, 0xff, 0xfb, 0xff, 0x07, 0xf8, 0xbf, 0x3d};
    printf("# %s\n", __FUNCTION__);

    memcpy(set_event_mask->events, events, sizeof(*set_event_mask));

    bt_hci_cmd(BT_HCI_OP_SET_EVENT_MASK, sizeof(*set_event_mask));
}

void bt_hci_cmd_reset(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_RESET, 0);
}

void bt_hci_cmd_set_event_filter(void *cp) {
    struct bt_hci_cp_set_event_filter *set_event_filter = (struct bt_hci_cp_set_event_filter *)&bt_hci_tx_pkt_tmp.cp;
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

void bt_hci_cmd_read_stored_link_key(void *cp) {
    struct bt_hci_cp_read_stored_link_key *read_stored_link_key = (struct bt_hci_cp_read_stored_link_key *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memset((void *)&read_stored_link_key->bdaddr, 0, sizeof(read_stored_link_key->bdaddr));
    read_stored_link_key->read_all_flag = 0x01;

    bt_hci_cmd(BT_HCI_OP_READ_STORED_LINK_KEY, sizeof(*read_stored_link_key));
}

void bt_hci_cmd_delete_stored_link_key(void *cp) {
    struct bt_hci_cp_delete_stored_link_key *delete_stored_link_key = (struct bt_hci_cp_delete_stored_link_key *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memset((void *)&delete_stored_link_key->bdaddr, 0, sizeof(delete_stored_link_key->bdaddr));
    delete_stored_link_key->delete_all_flag = 0x01;

    bt_hci_cmd(BT_HCI_OP_DELETE_STORED_LINK_KEY, sizeof(*delete_stored_link_key));
}

void bt_hci_cmd_write_local_name(void *cp) {
    struct bt_hci_cp_write_local_name *write_local_name = (struct bt_hci_cp_write_local_name *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memset((void *)write_local_name, 0, sizeof(*write_local_name));
    snprintf((char *)write_local_name->local_name, sizeof(write_local_name->local_name), "BlueRetro Adapter");

    bt_hci_cmd(BT_HCI_OP_WRITE_LOCAL_NAME, sizeof(*write_local_name));
}

void bt_hci_cmd_read_local_name(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_NAME, 0);
}

void bt_hci_cmd_write_conn_accept_timeout(void *cp) {
    struct bt_hci_cp_write_conn_accept_timeout *write_conn_accept_timeout = (struct bt_hci_cp_write_conn_accept_timeout *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_conn_accept_timeout->conn_accept_timeout = 0x7d00;

    bt_hci_cmd(BT_HCI_OP_WRITE_CONN_ACCEPT_TIMEOUT, sizeof(*write_conn_accept_timeout));
}

void bt_hci_cmd_write_page_scan_timeout(void *cp) {
    struct bt_hci_cp_write_page_scan_timeout *write_page_scan_timeout = (struct bt_hci_cp_write_page_scan_timeout *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_page_scan_timeout->timeout = 0x2000;

    bt_hci_cmd(BT_HCI_OP_WRITE_PAGE_TIMEOUT, sizeof(*write_page_scan_timeout));
}

void bt_hci_cmd_write_scan_enable(void *cp) {
    struct bt_hci_cp_write_scan_enable *write_scan_enable = (struct bt_hci_cp_write_scan_enable *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_scan_enable->scan_enable = BT_BREDR_SCAN_INQUIRY | BT_BREDR_SCAN_PAGE;

    bt_hci_cmd(BT_HCI_OP_WRITE_SCAN_ENABLE, sizeof(*write_scan_enable));
}

void bt_hci_cmd_write_page_scan_activity(void *cp) {
    struct bt_hci_cp_write_page_scan_activity *write_page_scan_activity = (struct bt_hci_cp_write_page_scan_activity *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_page_scan_activity->interval = 0x50;
    write_page_scan_activity->window = 0x12;

    bt_hci_cmd(BT_HCI_OP_WRITE_PAGE_SCAN_ACTIVITY, sizeof(*write_page_scan_activity));
}

void bt_hci_cmd_write_inquiry_scan_activity(void *cp) {
    struct bt_hci_cp_write_inquiry_scan_activity *write_inquiry_scan_activity = (struct bt_hci_cp_write_inquiry_scan_activity *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_inquiry_scan_activity->interval = 0x50;
    write_inquiry_scan_activity->window = 0x12;

    bt_hci_cmd(BT_HCI_OP_WRITE_INQUIRY_SCAN_ACTIVITY, sizeof(*write_inquiry_scan_activity));
}

void bt_hci_cmd_write_auth_enable(void *cp) {
    struct bt_hci_cp_write_auth_enable *write_auth_enable = (struct bt_hci_cp_write_auth_enable *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_auth_enable->auth_enable = 0x00;

    bt_hci_cmd(BT_HCI_OP_WRITE_AUTH_ENABLE, sizeof(*write_auth_enable));
}

void bt_hci_cmd_read_page_scan_activity(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_PAGE_SCAN_ACTIVITY, 0);
}

void bt_hci_cmd_read_class_of_device(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_CLASS_OF_DEVICE, 0);
}

void bt_hci_cmd_write_class_of_device(void *cp) {
    struct bt_hci_cp_write_class_of_device *write_class_of_device = (struct bt_hci_cp_write_class_of_device *)&bt_hci_tx_pkt_tmp.cp;
    bt_class_t local_class = {{0x0c, 0x01, 0x1c}}; /* Laptop */
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&write_class_of_device->dev_class, (void *)&local_class, sizeof(local_class));

    bt_hci_cmd(BT_HCI_OP_WRITE_CLASS_OF_DEVICE, sizeof(*write_class_of_device));
}

void bt_hci_cmd_read_voice_setting(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_VOICE_SETTING, 0);
}

void bt_hci_cmd_write_hold_mode_act(void *cp) {
    struct bt_hci_cp_write_hold_mode_act *write_hold_mode_act = (struct bt_hci_cp_write_hold_mode_act *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_hold_mode_act->activity = 0x00;

    bt_hci_cmd(BT_HCI_OP_WRITE_HOLD_MODE_ACT, sizeof(*write_hold_mode_act));
}

void bt_hci_cmd_read_num_supported_iac(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_NUM_SUPPORTED_IAC, 0);
}

void bt_hci_cmd_read_current_iac_lap(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_CURRENT_IAC_LAP, 0);
}

void bt_hci_cmd_write_inquiry_mode(void *cp) {
    struct bt_hci_cp_write_inquiry_mode *write_inquiry_mode = (struct bt_hci_cp_write_inquiry_mode *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_inquiry_mode->mode = 0x02;

    bt_hci_cmd(BT_HCI_OP_WRITE_INQUIRY_MODE, sizeof(*write_inquiry_mode));
}

void bt_hci_cmd_read_page_scan_type(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_PAGE_SCAN_TYPE, 0);
}

void bt_hci_cmd_write_page_scan_type(void *cp) {
    struct bt_hci_cp_write_page_scan_type *write_page_scan_type = (struct bt_hci_cp_write_page_scan_type *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_page_scan_type->type = 0x01;

    bt_hci_cmd(BT_HCI_OP_WRITE_PAGE_SCAN_TYPE, sizeof(*write_page_scan_type));
}

void bt_hci_cmd_write_ssp_mode(void *cp) {
    struct bt_hci_cp_write_ssp_mode *write_ssp_mode = (struct bt_hci_cp_write_ssp_mode *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_ssp_mode->mode = 0x01;

    bt_hci_cmd(BT_HCI_OP_WRITE_SSP_MODE, sizeof(*write_ssp_mode));
}

void bt_hci_cmd_read_inquiry_rsp_tx_pwr_lvl(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_INQUIRY_RSP_TX_PWR_LVL, 0);
}

void bt_hci_cmd_write_le_host_supp(void *cp) {
    struct bt_hci_cp_write_le_host_supp *write_le_host_supp = (struct bt_hci_cp_write_le_host_supp *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    write_le_host_supp->le = 0x00;
    write_le_host_supp->simul = 0x00;

    bt_hci_cmd(BT_HCI_OP_LE_WRITE_LE_HOST_SUPP, sizeof(*write_le_host_supp));
}

void bt_hci_cmd_read_local_version_info(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_VERSION_INFO, 0);
}

void bt_hci_cmd_read_supported_commands(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_SUPPORTED_COMMANDS, 0);
}

void bt_hci_cmd_read_local_features(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_FEATURES, 0);
}

void bt_hci_cmd_read_local_ext_features(void *cp) {
    struct bt_hci_cp_read_local_ext_features *read_local_ext_features = (struct bt_hci_cp_read_local_ext_features *)&bt_hci_tx_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    read_local_ext_features->page = 0x01;

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_EXT_FEATURES, sizeof(*read_local_ext_features));
}

void bt_hci_cmd_read_buffer_size(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_BUFFER_SIZE, 0);
}

void bt_hci_cmd_read_bd_addr(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_BD_ADDR, 0);
}

void bt_hci_cmd_read_data_block_size(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_DATA_BLOCK_SIZE, 0);
}

void bt_hci_cmd_read_local_codecs(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_CODECS, 0);
}

void bt_hci_cmd_read_local_sp_options(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_READ_LOCAL_SP_OPTIONS, 0);
}
