/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include "host.h"
#include "l2cap.h"
#include "hci.h"
#include "att.h"
#include "att_hid.h"
#include "smp.h"
#include "tools/util.h"
#include "system/led.h"
#include "adapter/config.h"
#include "wired/wired_comm.h"
#include "zephyr/uuid.h"

#define BT_INQUIRY_MAX 10

typedef void (*bt_cmd_func_t)(void *param);

struct bt_hci_cmd_cp {
    bt_cmd_func_t cmd;
    void *cp;
};

struct bt_hci_le_cb {
    struct bt_dev *device;
    bt_hci_le_cb_t callback;
};

static const char bt_default_pin[][5] = {
    "0000",
    "1234",
    "1111",
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
static uint32_t inquiry_state = 0;
static uint32_t inquiry_override = 0;
static RingbufHandle_t randq_hdl, encryptq_hdl;
static char local_name[24] = "BlueRetro";

static void bt_hci_cmd(uint16_t opcode, uint32_t cp_len);
//static void bt_hci_cmd_inquiry(void *cp);
//static void bt_hci_cmd_inquiry_cancel(void *cp);
static void bt_hci_cmd_periodic_inquiry(void *cp);
static void bt_hci_cmd_exit_periodic_inquiry(void *cp);
static void bt_hci_cmd_connect(void *bdaddr);
static void bt_hci_cmd_disconnect(void *handle);
static void bt_hci_cmd_accept_conn_req(void *bdaddr);
static void bt_hci_cmd_link_key_neg_reply(void *bdaddr);
static void bt_hci_cmd_pin_code_reply(void *cp);
//static void bt_hci_cmd_pin_code_neg_reply(void *bdaddr);
static void bt_hci_cmd_auth_requested(void *handle);
static void bt_hci_cmd_set_conn_encrypt(void *handle);
static void bt_hci_cmd_remote_name_request(void *bdaddr);
static void bt_hci_cmd_read_remote_features(void *handle);
static void bt_hci_cmd_read_remote_ext_features(void *handle);
//static void bt_hci_cmd_read_remote_version_info(uint16_t handle);
static void bt_hci_cmd_io_capability_reply(void *bdaddr);
static void bt_hci_cmd_user_confirm_reply(void *bdaddr);
static void bt_hci_cmd_exit_sniff_mode(void *handle);
static void bt_hci_cmd_switch_role(void *cp);
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
static void bt_hci_cmd_le_read_buffer_size(void *cp);
static void bt_hci_cmd_le_set_adv_param(void *cp);
static void bt_hci_cmd_le_set_adv_data(void *cp);
static void bt_hci_cmd_le_set_scan_rsp_data(void *cp);
static void bt_hci_cmd_le_set_adv_enable(void *cp);
static void bt_hci_cmd_le_set_scan_param_active(void);
static void bt_hci_cmd_le_set_scan_param_passive(void);
static void bt_hci_cmd_le_set_scan_enable(uint32_t enable);
static void bt_hci_cmd_le_create_conn(void *bdaddr_le);
static void bt_hci_cmd_le_read_wl_size(void *cp);
static void bt_hci_cmd_le_clear_wl(void *cp);
static void bt_hci_cmd_le_add_dev_to_wl(void *bdaddr_le);
static void bt_hci_cmd_le_conn_update(struct hci_cp_le_conn_update *cp);
//static void bt_hci_cmd_le_read_remote_features(uint16_t handle);
static void bt_hci_cmd_le_encrypt(const uint8_t *key, uint8_t *plaintext);
static void bt_hci_cmd_le_rand(void);
static void bt_hci_cmd_le_start_encryption(uint16_t handle, uint64_t rand, uint16_t ediv, uint8_t *ltk);
//static void bt_hci_cmd_le_set_ext_scan_param(void *cp);
//static void bt_hci_cmd_le_set_ext_scan_enable(void *cp);
static void bt_hci_le_meta_evt_hdlr(struct bt_hci_pkt *bt_hci_evt_pkt);
static void bt_hci_start_inquiry_cfg_check(void *cp);
static void bt_hci_load_le_accept_list(void *cp);
static void bt_hci_set_device_name(void);

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
    {bt_hci_cmd_le_read_wl_size, NULL},
    {bt_hci_cmd_le_clear_wl, NULL},
    {bt_hci_load_le_accept_list, NULL},
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
    {bt_hci_cmd_le_read_buffer_size, NULL},
    {bt_hci_cmd_le_set_adv_param, NULL},
    {bt_hci_cmd_le_set_adv_data, NULL},
    {bt_hci_cmd_le_set_scan_rsp_data, NULL},
    {bt_hci_cmd_le_set_adv_enable, NULL},
    {bt_hci_start_inquiry_cfg_check, NULL},
};

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

static void bt_hci_cmd_disconnect(void *handle) {
    struct bt_hci_cp_disconnect *disconnect = (struct bt_hci_cp_disconnect *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    disconnect->handle = *(uint16_t *)handle;
    //disconnect->reason = BT_HCI_ERR_REMOTE_USER_TERM_CONN;
    disconnect->reason = BT_HCI_ERR_REMOTE_POWER_OFF;

    bt_hci_cmd(BT_HCI_OP_DISCONNECT, sizeof(*disconnect));
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

#if 0
static void bt_hci_cmd_read_remote_version_info(uint16_t handle) {
    struct bt_hci_cp_read_remote_version_info *read_remote_version_info = (struct bt_hci_cp_read_remote_version_info *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    read_remote_version_info->handle = handle;

    bt_hci_cmd(BT_HCI_OP_READ_REMOTE_VERSION_INFO, sizeof(*read_remote_version_info));
}
#endif

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

static void bt_hci_cmd_exit_sniff_mode(void *handle) {
    struct bt_hci_cp_exit_sniff_mode *exit_sniff_mode = (struct bt_hci_cp_exit_sniff_mode *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    exit_sniff_mode->handle = *(uint16_t *)handle;

    bt_hci_cmd(BT_HCI_OP_EXIT_SNIFF_MODE, sizeof(*exit_sniff_mode));
}

static void bt_hci_cmd_switch_role(void *cp) {
    struct bt_hci_cp_switch_role *switch_role = (struct bt_hci_cp_switch_role *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)switch_role, cp, sizeof(*switch_role));

    bt_hci_cmd(BT_HCI_OP_SWITCH_ROLE, sizeof(*switch_role));
}

#if 0
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

    write_le_host_supp->le = 0x01;
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

static void bt_hci_cmd_le_read_buffer_size(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_LE_READ_BUFFER_SIZE, 0);
}

static void bt_hci_cmd_le_set_adv_param(void *cp) {
    struct bt_hci_cp_le_set_adv_param *le_set_adv_param = (struct bt_hci_cp_le_set_adv_param *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    le_set_adv_param->min_interval = 0x00A0;
    le_set_adv_param->max_interval = 0x00A0;
    le_set_adv_param->type = 0x00;
    le_set_adv_param->own_addr_type = 0x00;
    memset((void *)&le_set_adv_param->direct_addr, 0, sizeof(le_set_adv_param->direct_addr));
    le_set_adv_param->channel_map = 0x07;
    le_set_adv_param->filter_policy = 0x00;

    bt_hci_cmd(BT_HCI_OP_LE_SET_ADV_PARAM, sizeof(*le_set_adv_param));
}

static void bt_hci_cmd_le_set_adv_data(void *cp) {
    uint8_t adv_data[32] = {
        0x02, BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
        0x03, BT_DATA_UUID16_SOME, 0x0f, 0x18,
        0x0a, BT_DATA_NAME_COMPLETE, 0x00
    };
    struct bt_hci_cp_le_set_adv_data *le_set_adv_data = (struct bt_hci_cp_le_set_adv_data *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    adv_data[7] = strlen(bt_hci_get_device_name()) + 1;
    strncat((char *)adv_data, bt_hci_get_device_name(), 22);

    memset(le_set_adv_data->data, 0, sizeof(le_set_adv_data->data));
    le_set_adv_data->len = strlen((char *)adv_data);
    memcpy(le_set_adv_data->data, adv_data, strlen((char *)adv_data));

    bt_hci_cmd(BT_HCI_OP_LE_SET_ADV_DATA, sizeof(*le_set_adv_data));
}

static void bt_hci_cmd_le_set_scan_rsp_data(void *cp) {
    uint8_t scan_rsp[] = {
        0x11, BT_DATA_UUID128_ALL, 0x56, 0x9a, 0x79, 0x76, 0xa1, 0x2f, 0x4b, 0x31, 0xb0, 0xfa, 0x80, 0x51, 0x56, 0x0f, 0x83, 0x00
    };
    struct bt_hci_cp_le_set_scan_rsp_data *le_set_scan_rsp_data = (struct bt_hci_cp_le_set_scan_rsp_data *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memset(le_set_scan_rsp_data->data, 0, sizeof(le_set_scan_rsp_data->data));
    le_set_scan_rsp_data->len = sizeof(scan_rsp);
    memcpy(le_set_scan_rsp_data->data, scan_rsp, sizeof(scan_rsp));

    bt_hci_cmd(BT_HCI_OP_LE_SET_SCAN_RSP_DATA, sizeof(*le_set_scan_rsp_data));
}

static void bt_hci_cmd_le_set_adv_enable(void *cp) {
    struct bt_hci_cp_le_set_adv_enable *le_set_adv_enable = (struct bt_hci_cp_le_set_adv_enable *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    le_set_adv_enable->enable = 0x01;

    bt_hci_cmd(BT_HCI_OP_LE_SET_ADV_ENABLE, sizeof(*le_set_adv_enable));
}

static void bt_hci_cmd_le_set_adv_disable(void *cp) {
    struct bt_hci_cp_le_set_adv_enable *le_set_adv_enable = (struct bt_hci_cp_le_set_adv_enable *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    le_set_adv_enable->enable = 0x00;

    bt_hci_cmd(BT_HCI_OP_LE_SET_ADV_ENABLE, sizeof(*le_set_adv_enable));
}

static void bt_hci_cmd_le_set_scan_param_active(void) {
    struct bt_hci_cp_le_set_scan_param *le_set_scan_param = (struct bt_hci_cp_le_set_scan_param *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    le_set_scan_param->scan_type = BT_HCI_LE_SCAN_ACTIVE;
    le_set_scan_param->interval = 36;
    le_set_scan_param->window = 18;
    le_set_scan_param->addr_type = 0x00;
    le_set_scan_param->filter_policy = 0x00;

    bt_hci_cmd(BT_HCI_OP_LE_SET_SCAN_PARAM, sizeof(*le_set_scan_param));
}

static void bt_hci_cmd_le_set_scan_param_passive(void) {
    struct bt_hci_cp_le_set_scan_param *le_set_scan_param = (struct bt_hci_cp_le_set_scan_param *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    le_set_scan_param->scan_type = BT_HCI_LE_SCAN_PASSIVE;
    le_set_scan_param->interval = 1024;
    le_set_scan_param->window = 18;
    le_set_scan_param->addr_type = 0x00;
    le_set_scan_param->filter_policy = 0x01;

    bt_hci_cmd(BT_HCI_OP_LE_SET_SCAN_PARAM, sizeof(*le_set_scan_param));
}

static void bt_hci_cmd_le_set_scan_enable(uint32_t enable) {
    struct bt_hci_cp_le_set_scan_enable *le_set_scan_enable = (struct bt_hci_cp_le_set_scan_enable *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    le_set_scan_enable->enable = enable;
    le_set_scan_enable->filter_dup = 0x01;

    bt_hci_cmd(BT_HCI_OP_LE_SET_SCAN_ENABLE, sizeof(*le_set_scan_enable));
}

static void bt_hci_cmd_le_create_conn(void *bdaddr_le) {
    struct bt_hci_cp_le_create_conn *le_create_conn = (struct bt_hci_cp_le_create_conn *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&le_create_conn->peer_addr, bdaddr_le, sizeof(le_create_conn->peer_addr));

    le_create_conn->scan_interval = 18;
    le_create_conn->scan_window = 18;
    le_create_conn->filter_policy = 0x00;
    le_create_conn->own_addr_type = 0x00;
    le_create_conn->conn_interval_min = 6;
    le_create_conn->conn_interval_max = 12;
    le_create_conn->conn_latency = 0;
    le_create_conn->supervision_timeout = 300;
    le_create_conn->min_ce_len = 0;
    le_create_conn->max_ce_len = 0;

    bt_hci_cmd(BT_HCI_OP_LE_CREATE_CONN, sizeof(*le_create_conn));
}

static void bt_hci_cmd_le_read_wl_size(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_LE_READ_WL_SIZE, 0);
}

static void bt_hci_cmd_le_clear_wl(void *cp) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_LE_CLEAR_WL, 0);
}

static void bt_hci_cmd_le_add_dev_to_wl(void *bdaddr_le) {
    struct bt_hci_cp_le_add_dev_to_wl *le_add_dev_to_wl = (struct bt_hci_cp_le_add_dev_to_wl *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&le_add_dev_to_wl->addr, bdaddr_le, sizeof(le_add_dev_to_wl->addr));

    bt_hci_cmd(BT_HCI_OP_LE_ADD_DEV_TO_WL, sizeof(*le_add_dev_to_wl));
}

static void bt_hci_cmd_le_conn_update(struct hci_cp_le_conn_update *cp) {
    struct hci_cp_le_conn_update *le_conn_update = (struct hci_cp_le_conn_update *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((uint8_t *)le_conn_update, (uint8_t *)cp, sizeof(*le_conn_update));

    bt_hci_cmd(BT_HCI_OP_LE_CONN_UPDATE, sizeof(*le_conn_update));
}

#if 0
static void bt_hci_cmd_le_read_remote_features(uint16_t handle) {
    struct bt_hci_cp_le_read_remote_features *le_read_remote_features =
        (struct bt_hci_cp_le_read_remote_features *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    le_read_remote_features->handle = handle;

    bt_hci_cmd(BT_HCI_OP_LE_READ_REMOTE_FEATURES, sizeof(*le_read_remote_features));
}
#endif

static void bt_hci_cmd_le_encrypt(const uint8_t *key, uint8_t *plaintext) {
    struct bt_hci_cp_le_encrypt *le_encrypt = (struct bt_hci_cp_le_encrypt *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&le_encrypt->key, key, sizeof(le_encrypt->key));
    memcpy((void *)&le_encrypt->plaintext, plaintext, sizeof(le_encrypt->plaintext));

    bt_hci_cmd(BT_HCI_OP_LE_ENCRYPT, sizeof(*le_encrypt));
}

static void bt_hci_cmd_le_rand(void) {
    printf("# %s\n", __FUNCTION__);

    bt_hci_cmd(BT_HCI_OP_LE_RAND, 0);
}

static void bt_hci_cmd_le_start_encryption(uint16_t handle, uint64_t rand, uint16_t ediv, uint8_t *ltk) {
    struct bt_hci_cp_le_start_encryption *le_start_encryption = (struct bt_hci_cp_le_start_encryption *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    le_start_encryption->handle = handle;
    le_start_encryption->rand = rand;
    le_start_encryption->ediv = ediv;
    memcpy((void *)&le_start_encryption->ltk, ltk, sizeof(le_start_encryption->ltk));

    bt_hci_cmd(BT_HCI_OP_LE_START_ENCRYPTION, sizeof(*le_start_encryption));
}

#if 0
static void bt_hci_cmd_le_set_ext_scan_param(void *cp) {
    struct bt_hci_cp_le_set_ext_scan_param *le_set_ext_scan_param = (struct bt_hci_cp_le_set_ext_scan_param *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    le_set_ext_scan_param->own_addr_type = 0x01;
    le_set_ext_scan_param->filter_policy = 0x00;
    le_set_ext_scan_param->phys = BT_HCI_LE_EXT_SCAN_PHY_1M;
    le_set_ext_scan_param->p[0].type = 0x01;
    le_set_ext_scan_param->p[0].interval = 6553;
    le_set_ext_scan_param->p[0].window = 1638;

    bt_hci_cmd(BT_HCI_OP_LE_SET_EXT_SCAN_PARAM, sizeof(*le_set_ext_scan_param) - sizeof(le_set_ext_scan_param->p) + sizeof(*le_set_ext_scan_param->p));
}

static void bt_hci_cmd_le_set_ext_scan_enable(void *cp) {
    struct bt_hci_cp_le_set_ext_scan_enable *le_set_ext_scan_enable = (struct bt_hci_cp_le_set_ext_scan_enable *)&bt_hci_pkt_tmp.cp;
    printf("# %s\n", __FUNCTION__);

    le_set_ext_scan_enable->enable = (uint32_t)cp;
    le_set_ext_scan_enable->filter_dup = 0x01;
    le_set_ext_scan_enable->duration = 0x00;
    le_set_ext_scan_enable->period = 0x00;

    bt_hci_cmd(BT_HCI_OP_LE_SET_EXT_SCAN_ENABLE, sizeof(*le_set_ext_scan_enable));
}
#endif

static void bt_hci_le_meta_evt_hdlr(struct bt_hci_pkt *bt_hci_evt_pkt) {
    struct bt_hci_evt_le_meta_event *le_meta_event = (struct bt_hci_evt_le_meta_event *)bt_hci_evt_pkt->evt_data;
    struct bt_dev *device = NULL;

    switch (le_meta_event->subevent) {
        case BT_HCI_EVT_LE_CONN_COMPLETE:
        case BT_HCI_EVT_LE_ENH_CONN_COMPLETE:
        {
            struct bt_hci_evt_le_conn_complete *le_conn_complete =
                (struct bt_hci_evt_le_conn_complete *)(bt_hci_evt_pkt->evt_data + sizeof(struct bt_hci_evt_le_meta_event));
            printf("# BT_HCI_EVT_LE_CONN_COMPLETE\n");
            bt_host_get_dev_from_bdaddr(le_conn_complete->peer_addr.a.val, &device);
            if (device) {
                if (le_conn_complete->status) {
                    device->pkt_retry++;
                    printf("# dev: %ld error: 0x%02X\n", device->ids.id, le_conn_complete->status);
                    if (!atomic_test_bit(&device->flags, BT_DEV_PAGE) && device->pkt_retry < BT_MAX_RETRY) {
                        bt_hci_cmd_le_create_conn((void *)&le_conn_complete->peer_addr);
                    }
                    else {
                        bt_host_reset_dev(device);
                        if (config.global_cfg.inquiry_mode == INQ_AUTO && bt_host_get_active_dev(&device) == BT_NONE) {
                            bt_hci_start_inquiry();;
                        }
                        else {
                            bt_hci_cmd_le_set_scan_enable(0);
                            bt_hci_cmd_le_set_scan_param_passive();
                            bt_hci_cmd_le_set_scan_enable(1);
                        }
                    }
                }
                else {
                    struct bt_smp_encrypt_info encrypt_info = {0};
                    struct bt_smp_master_ident master_ident = {0};
                    bt_nb_inquiry = BT_INQUIRY_MAX;
                    device->acl_handle = le_conn_complete->handle;
                    device->pkt_retry = 0;
                    printf("# dev: %ld acl_handle: 0x%04X\n", device->ids.id, device->acl_handle);
                    atomic_set_bit(&device->flags, BT_DEV_IS_BLE);
                    if (atomic_test_bit(&device->flags, BT_DEV_PAGE) && bt_host_load_le_ltk(&device->le_remote_bdaddr, &encrypt_info, &master_ident) == 0) {
                        bt_hci_start_encryption(device->acl_handle, *(uint64_t *)master_ident.rand, *(uint16_t *)master_ident.ediv, encrypt_info.ltk);
                        bt_att_hid_init(device);
                    }
                    else {
                        bt_smp_pairing_start(device);
                    }
                    bt_hci_cmd_le_set_scan_enable(0);
                    bt_hci_cmd_le_set_scan_param_active();
                    bt_hci_cmd_le_set_scan_enable(1);
                }
            }
            else {
                bt_host_get_dev_conf(&device);

                if (!le_conn_complete->status && !atomic_test_bit(&device->flags, BT_DEV_DEVICE_FOUND)) {
                    atomic_set_bit(&device->flags, BT_DEV_DEVICE_FOUND);
                    device->acl_handle = le_conn_complete->handle;
                }
                else {
                    printf("# dev NULL!\n");
                }
            }
            break;
        }
        case BT_HCI_EVT_LE_ADVERTISING_REPORT:
        {
            struct bt_hci_evt_le_advertising_report *le_adv_report =
                (struct bt_hci_evt_le_advertising_report *)(bt_hci_evt_pkt->evt_data + sizeof(struct bt_hci_evt_le_meta_event));
                uint8_t *data = le_adv_report->adv_info[0].data;
                uint8_t *end = data + le_adv_report->adv_info[0].length;
                uint8_t len, type;
                uint16_t value;

                printf("# BT_HCI_EVT_LE_ADVERTISING_REPORT\n");

                if (le_adv_report->adv_info[0].evt_type == BT_LE_ADV_DIRECT_IND) {
                    goto connect;
                }

                /* Filter HID device */
                while (data < end) {
                    len = *data++;
                    type = *data;
                    switch (type) {
                        case BT_DATA_UUID16_SOME:
                        case BT_DATA_UUID16_ALL:
                            value = *(uint16_t *)&data[1];
                            if (value == BT_UUID_HIDS) {
                                goto connect;
                            }
                            else {
                                goto skip;
                            }
                            break;
                        case BT_DATA_GAP_APPEARANCE:
                            /* HID category */
                            value = *(uint16_t *)&data[1] & 0xFFC0;
                            if (value == 0x03C0) {
                                goto connect;
                            }
                            else {
                                goto skip;
                            }
                            break;
                    }
                    data += len;
                }
                goto skip;
connect:
                bt_host_get_dev_from_bdaddr(le_adv_report->adv_info[0].addr.a.val, &device);
                if (device == NULL) {
                    (void)bt_host_get_new_dev(&device);
                    if (device) {
                        bt_host_reset_dev(device);
                        if (le_adv_report->adv_info[0].evt_type == BT_LE_ADV_DIRECT_IND) {
                            atomic_set_bit(&device->flags, BT_DEV_PAGE);
                        }
                        memcpy((uint8_t *)&device->le_remote_bdaddr, (uint8_t *)&le_adv_report->adv_info[0].addr, sizeof(device->le_remote_bdaddr));
                        device->ids.type = BT_HID_GENERIC;
                        bt_l2cap_init_dev_scid(device);
                        atomic_set_bit(&device->flags, BT_DEV_DEVICE_FOUND);
                        bt_hci_cmd_le_set_scan_enable(0);
                        bt_hci_cmd_le_set_adv_disable(NULL);
                        bt_hci_cmd_le_create_conn((void *)&le_adv_report->adv_info[0].addr);
                        printf("# LE ADV dev: %ld type: %ld bdaddr: %02X - %02X:%02X:%02X:%02X:%02X:%02X\n", device->ids.id, device->ids.type, device->le_remote_bdaddr.type,
                            device->remote_bdaddr[5], device->remote_bdaddr[4], device->remote_bdaddr[3],
                            device->remote_bdaddr[2], device->remote_bdaddr[1], device->remote_bdaddr[0]);
                    }
                }
skip:
            break;
        }
        case BT_HCI_EV_LE_REMOTE_FEAT_COMPLETE:
        {
            struct bt_hci_evt_le_remote_feat_complete *le_remote_feat =
                (struct bt_hci_evt_le_remote_feat_complete *)(bt_hci_evt_pkt->evt_data + sizeof(struct bt_hci_evt_le_meta_event));
            printf("# BT_HCI_EVT_LE_REMOTE_FEAT_COMPLETE\n");
            bt_host_get_dev_from_handle(le_remote_feat->handle, &device);
            if (device) {
                if (le_remote_feat->status) {
                    printf("# dev: %ld error: 0x%02X\n", device->ids.id, le_remote_feat->status);
                }
            }
            else {
                printf("# dev NULL!\n");
            }
            break;
        }
    }
}

static void bt_hci_start_inquiry_cfg_check(void *cp) {
    if (config.global_cfg.inquiry_mode == INQ_AUTO) {
        bt_hci_start_inquiry();
    }
    else {
        bt_hci_cmd_le_set_scan_enable(0);
        bt_hci_cmd_le_set_scan_param_passive();
        bt_hci_cmd_le_set_scan_enable(1);
    }
}

static void bt_hci_load_le_accept_list(void *cp) {
    bt_addr_le_t le_bdaddr;
    if (bt_host_get_next_accept_le_bdaddr(&le_bdaddr) == 0) {
        bt_hci_cmd_le_add_dev_to_wl(&le_bdaddr);
    }
    else {
        bt_hci_pkt_retry = 0;
        bt_hci_q_conf(1);
    }
}

static int32_t bt_hci_get_random_context(struct bt_dev **device, bt_hci_le_cb_t *cb) {
    size_t len;
    struct bt_hci_le_cb *tmp = (struct bt_hci_le_cb *)xRingbufferReceive(randq_hdl, &len, 0);
    if (tmp) {
        *device = tmp->device;
        *cb = tmp->callback;
        vRingbufferReturnItem(randq_hdl, (void *)tmp);
        return 0;
    }
    return -1;
}

static int32_t bt_hci_get_encrypt_context(struct bt_dev **device, bt_hci_le_cb_t *cb) {
    size_t len;
    struct bt_hci_le_cb *tmp = (struct bt_hci_le_cb *)xRingbufferReceive(encryptq_hdl, &len, 0);
    if (tmp) {
        *device = tmp->device;
        *cb = tmp->callback;
        vRingbufferReturnItem(encryptq_hdl, (void *)tmp);
        return 0;
    }
    return -1;
}

static void bt_hci_set_device_name(void) {
    strcat(local_name, "_");
    strncat(local_name, wired_get_sys_name(), 6);
    strcat(local_name, "_");
    snprintf(local_name + strlen(local_name), 5, "%02X%02X", local_bdaddr[1], local_bdaddr[0]);
    printf("# local_name: %s\n", local_name);
}

int32_t bt_hci_init(void) {
    randq_hdl = xRingbufferCreate(sizeof(struct bt_hci_le_cb) * 8, RINGBUF_TYPE_NOSPLIT);
    if (randq_hdl == NULL) {
        printf("# Failed to create randq ring buffer\n");
        return -1;
    }

    encryptq_hdl = xRingbufferCreate(sizeof(struct bt_hci_le_cb) * 8, RINGBUF_TYPE_NOSPLIT);
    if (encryptq_hdl == NULL) {
        printf("# Failed to create randq ring buffer\n");
        return -1;
    }

    bt_config_state = 0;
    bt_hci_q_conf(0);

    return 0;
}

const char *bt_hci_get_device_name(void) {
    return local_name;
}

void bt_hci_start_inquiry(void) {
    if (!inquiry_override) {
        err_led_pulse();
        inquiry_state = 1;
        bt_nb_inquiry = 0;
        bt_hci_cmd_le_set_scan_enable(0);
        bt_hci_cmd_le_set_scan_param_active();
        bt_hci_cmd_le_set_scan_enable(1);
        bt_hci_cmd_periodic_inquiry(NULL);
    }
}

void bt_hci_stop_inquiry(void) {
    bt_hci_cmd_exit_periodic_inquiry(NULL);
    bt_hci_cmd_le_set_scan_enable(0);
    bt_hci_cmd_le_set_scan_param_passive();
    bt_hci_cmd_le_set_scan_enable(1);
    inquiry_state = 0;
    err_led_clear();
}

uint32_t bt_hci_get_inquiry(void) {
    return inquiry_state;
}

void bt_hci_inquiry_override(uint32_t state) {
    inquiry_override = state;
}

void bt_hci_disconnect(struct bt_dev *device) {
    if (atomic_test_bit(&device->flags, BT_DEV_DEVICE_FOUND)) {
        bt_hci_cmd_disconnect(&device->acl_handle);
    }
}

void bt_hci_exit_sniff_mode(struct bt_dev *device) {
    bt_hci_cmd_exit_sniff_mode((void *)&device->acl_handle);
}

void bt_hci_get_le_local_addr(bt_addr_le_t *le_local) {
    memcpy(le_local->a.val, local_bdaddr, sizeof(le_local->a.val));
    le_local->type = 0x00;
}

int32_t bt_hci_get_random(struct bt_dev *device, bt_hci_le_cb_t cb) {
    struct bt_hci_le_cb le_cb = {
        .device = device,
        .callback = cb,
    };
    UBaseType_t ret = xRingbufferSend(randq_hdl, (void *)&le_cb, sizeof(le_cb), 0);
    if (ret != pdTRUE) {
        printf("# %s randq full!\n", __FUNCTION__);
        return -1;
    }

    bt_hci_cmd_le_rand();
    return 0;
}

int32_t bt_hci_get_encrypt(struct bt_dev *device, bt_hci_le_cb_t cb, const uint8_t *key, uint8_t *plaintext) {
    struct bt_hci_le_cb le_cb = {
        .device = device,
        .callback = cb,
    };
    UBaseType_t ret = xRingbufferSend(encryptq_hdl, (void *)&le_cb, sizeof(le_cb), 0);
    if (ret != pdTRUE) {
        printf("# %s encryptq full!\n", __FUNCTION__);
        return -1;
    }

    /* We hope here that the BT ctrl process encrypt req FIFO */
    bt_hci_cmd_le_encrypt(key, plaintext);
    return 0;
}

void bt_hci_start_encryption(uint16_t handle, uint64_t rand, uint16_t ediv, uint8_t *ltk) {
    bt_hci_cmd_le_start_encryption(handle, rand, ediv, ltk);
}

void bt_hci_add_to_accept_list(bt_addr_le_t *le_bdaddr) {
    bt_hci_cmd_le_add_dev_to_wl(le_bdaddr);
}

void bt_hci_le_conn_update(struct hci_cp_le_conn_update *cp) {
    bt_hci_cmd_le_conn_update(cp);
}

void bt_hci_evt_hdlr(struct bt_hci_pkt *bt_hci_evt_pkt) {
    struct bt_dev *device = NULL;

    switch (bt_hci_evt_pkt->evt_hdr.evt) {
        case BT_HCI_EVT_INQUIRY_COMPLETE:
            printf("# BT_HCI_EVT_INQUIRY_COMPLETE\n");
            bt_nb_inquiry++;
            if (bt_host_get_active_dev(&device) > -1 && bt_nb_inquiry > BT_INQUIRY_MAX) {
                bt_hci_stop_inquiry();
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
                bt_host_get_dev_from_bdaddr((uint8_t *)inquiry_result + 1, &device);
                if (device == NULL) {
                    (void)bt_host_get_new_dev(&device);
                    if (device) {
                        bt_host_reset_dev(device);
                        memcpy(device->remote_bdaddr, (uint8_t *)inquiry_result + 1, sizeof(device->remote_bdaddr));
                        device->ids.type = BT_HID_GENERIC;
                        bt_l2cap_init_dev_scid(device);
                        atomic_set_bit(&device->flags, BT_DEV_DEVICE_FOUND);
                        bt_hci_cmd_connect(device->remote_bdaddr);
                        printf("# Inquiry dev: %ld type: %ld bdaddr: %02X:%02X:%02X:%02X:%02X:%02X\n", device->ids.id, device->ids.type,
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
            bt_host_get_dev_from_bdaddr(conn_complete->bdaddr.val, &device);
            if (device) {
                if (conn_complete->status) {
                    device->pkt_retry++;
                    printf("# dev: %ld error: 0x%02X\n", device->ids.id, conn_complete->status);
                    if (!atomic_test_bit(&device->flags, BT_DEV_PAGE) && device->pkt_retry < BT_MAX_RETRY) {
                        bt_hci_cmd_connect(device->remote_bdaddr);
                    }
                    else {
                        bt_host_reset_dev(device);
                        if (config.global_cfg.inquiry_mode == INQ_AUTO && bt_host_get_active_dev(&device) == BT_NONE) {
                            bt_hci_start_inquiry();
                        }
                    }
                }
                else {
                    bt_nb_inquiry = BT_INQUIRY_MAX;
                    device->acl_handle = conn_complete->handle;
                    device->pkt_retry = 0;
                    printf("# dev: %ld acl_handle: 0x%04X\n", device->ids.id, device->acl_handle);
                    bt_hci_cmd_le_set_adv_disable(NULL);
                    bt_hci_cmd_remote_name_request(device->remote_bdaddr);
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
            bt_host_get_dev_from_bdaddr(conn_request->bdaddr.val, &device);
            if (device == NULL) {
                (void)bt_host_get_new_dev(&device);
                if (device) {
                    bt_host_reset_dev(device);
                    memcpy(device->remote_bdaddr, (void *)&conn_request->bdaddr, sizeof(device->remote_bdaddr));
                    device->ids.type = BT_HID_GENERIC;;
                    bt_l2cap_init_dev_scid(device);
                    atomic_set_bit(&device->flags, BT_DEV_DEVICE_FOUND);
                    atomic_set_bit(&device->flags, BT_DEV_PAGE);
                    bt_hci_cmd_accept_conn_req(device->remote_bdaddr);
                    printf("# Page dev: %ld type: %ld bdaddr: %02X:%02X:%02X:%02X:%02X:%02X\n", device->ids.id, device->ids.type,
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
            if (device) {
                printf("# DISCONN from dev: %ld\n", device->ids.id);
                bt_host_reset_dev(device);
                if (bt_host_get_active_dev(&device) == BT_NONE) {
                    if (config.global_cfg.inquiry_mode == INQ_AUTO) {
                        bt_hci_start_inquiry();
                    }
                    bt_hci_cmd_le_set_adv_enable(NULL);
                }
            }
            else {
                bt_host_get_dev_conf(&device);
                if (device && disconn_complete->handle == device->acl_handle) {
                    printf("# DISCONN from BLE config interface\n");
                    if (atomic_test_bit(&device->flags, BT_DEV_DEVICE_FOUND)) {
                        bt_host_reset_dev(device);
                        if (bt_host_get_active_dev(&device) == BT_NONE) {
                            bt_hci_cmd_le_set_adv_enable(NULL);
                        }
                    }
                }
                else {
                    printf("# DISCONN for non existing device handle: %04X\n", disconn_complete->handle);
                }
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
                    printf("# dev: %ld error: 0x%02X\n", device->ids.id, auth_complete->status);
                }
                else {
                    printf("# dev: %ld Pairing done\n", device->ids.id);
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
            bt_host_get_dev_from_bdaddr(remote_name_req_complete->bdaddr.val, &device);
            if (device) {
                if (remote_name_req_complete->status) {
                    device->pkt_retry++;
                    printf("# dev: %ld error: 0x%02X\n", device->ids.id, remote_name_req_complete->status);
                    if (device->pkt_retry < BT_MAX_RETRY) {
                        bt_hci_cmd_remote_name_request(device->remote_bdaddr);
                    }
                    else {
                        bt_host_reset_dev(device);
                        if (config.global_cfg.inquiry_mode == INQ_AUTO && bt_host_get_active_dev(&device) == BT_NONE) {
                            bt_hci_start_inquiry();
                        }
                    }
                }
                else {
                    bt_hid_set_type_flags_from_name(device, (char *)remote_name_req_complete->name);
                    bt_hci_stop_inquiry();
                    if (device->ids.type == BT_HID_GENERIC || device->ids.type == BT_SW) {
                        bt_hci_cmd_read_remote_features(&device->acl_handle);
                    }
                    if (device->ids.type == BT_PS3 || device->ids.subtype == BT_WIIU_PRO) {
                        struct bt_hci_cp_switch_role switch_role = {
                            .role = 0x01,
                        };
                        memcpy((void *)&switch_role.bdaddr, remote_name_req_complete->bdaddr.val, sizeof(remote_name_req_complete->bdaddr.val));
                        bt_hci_cmd_switch_role(&switch_role);
                    }
                    if (!atomic_test_bit(&device->flags, BT_DEV_PAGE)) {
                        if (device->ids.type == BT_PS) {
                            bt_host_q_wait_pkt(20);
                        }
                        bt_hci_cmd_auth_requested(&device->acl_handle);
                    }
                    printf("# dev: %ld type: %ld:%ld %s\n", device->ids.id, device->ids.type, device->ids.subtype, remote_name_req_complete->name);
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
                    printf("# dev: %ld error: 0x%02X\n", device->ids.id, encrypt_change->status);
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
                    printf("# dev: %ld error: 0x%02X\n", device->ids.id, remote_features->status);
                }
                else {
                    if (remote_features->features[7] & 0x80) {
                        bt_hci_cmd_read_remote_ext_features(&device->acl_handle);
                    }
                    if (remote_features->features[0] & 0x04) {
                        atomic_set_bit(&device->flags, BT_DEV_ENCRYPTION);
                    }
                }
                bt_l2cap_cmd_ext_feat_mask_req(device);
            }
            else {
                printf("# dev NULL!\n");
            }
            break;
        }
        case BT_HCI_EVT_REMOTE_VERSION_INFO:
        {
            struct bt_hci_evt_remote_version_info *remote_version_info = (struct bt_hci_evt_remote_version_info *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_REMOTE_VERSION_INFO\n");
            bt_host_get_dev_from_handle(remote_version_info->handle, &device);
            if (device) {
                if (remote_version_info->status) {
                    printf("# dev: %ld error: 0x%02X\n", device->ids.id, remote_version_info->status);
                }
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
                printf("# opcode: 0x%04X error: 0x%02X retry: %ld\n", cmd_complete->opcode, status, bt_hci_pkt_retry);
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
                    case BT_HCI_OP_LE_READ_BUFFER_SIZE:
                    case BT_HCI_OP_LE_WRITE_LE_HOST_SUPP:
                    case BT_HCI_OP_LE_READ_WL_SIZE:
                    case BT_HCI_OP_LE_CLEAR_WL:
                    case BT_HCI_OP_LE_ADD_DEV_TO_WL:
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
                    case BT_HCI_OP_LE_SET_SCAN_PARAM:
                    case BT_HCI_OP_LE_SET_SCAN_ENABLE:
                    case BT_HCI_OP_LE_SET_ADV_PARAM:
                    case BT_HCI_OP_LE_SET_ADV_DATA:
                    case BT_HCI_OP_LE_SET_SCAN_RSP_DATA:
                    case BT_HCI_OP_LE_SET_ADV_ENABLE:
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
                    case BT_HCI_OP_LE_READ_BUFFER_SIZE:
                    {
                        struct bt_hci_rp_le_read_buffer_size *le_read_buffer_size = (struct bt_hci_rp_le_read_buffer_size *)&bt_hci_evt_pkt->evt_data[sizeof(*cmd_complete)];
                        bt_att_set_le_max_mtu(le_read_buffer_size->le_max_len - sizeof(struct bt_l2cap_hdr));
                        bt_hci_pkt_retry = 0;
                        bt_hci_q_conf(1);
                        break;
                    }
                    case BT_HCI_OP_LE_READ_WL_SIZE:
                    {
                        //struct bt_hci_rp_le_read_wl_size *le_read_wl_size = (struct bt_hci_rp_le_read_wl_size *)&bt_hci_evt_pkt->evt_data[sizeof(*cmd_complete)];
                        //TODO use value
                        bt_hci_pkt_retry = 0;
                        bt_hci_q_conf(1);
                        break;
                    }
                    case BT_HCI_OP_LE_ADD_DEV_TO_WL:
                    {
                        bt_addr_le_t le_bdaddr;
                        if (bt_host_get_next_accept_le_bdaddr(&le_bdaddr) == 0) {
                            bt_hci_cmd_le_add_dev_to_wl(&le_bdaddr);
                        }
                        else {
                            bt_hci_pkt_retry = 0;
                            bt_hci_q_conf(1);
                        }
                        break;
                    }
                    case BT_HCI_OP_READ_BD_ADDR:
                    {
                        struct bt_hci_rp_read_bd_addr *read_bd_addr = (struct bt_hci_rp_read_bd_addr *)&bt_hci_evt_pkt->evt_data[sizeof(*cmd_complete)];
                        memcpy((void *)local_bdaddr, (void *)&read_bd_addr->bdaddr, sizeof(local_bdaddr));
                        bt_hci_set_device_name();
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
                    case BT_HCI_OP_LE_CLEAR_WL:
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
                    case BT_HCI_OP_LE_SET_SCAN_PARAM:
                    case BT_HCI_OP_LE_SET_SCAN_ENABLE:
                    case BT_HCI_OP_LE_SET_ADV_PARAM:
                    case BT_HCI_OP_LE_SET_ADV_DATA:
                    case BT_HCI_OP_LE_SET_SCAN_RSP_DATA:
                    case BT_HCI_OP_LE_SET_ADV_ENABLE:
                        bt_hci_pkt_retry = 0;
                        bt_hci_q_conf(1);
                        break;
                    case BT_HCI_OP_LE_RAND:
                    {
                        struct bt_hci_rp_le_rand *le_rand =
                            (struct bt_hci_rp_le_rand *)&bt_hci_evt_pkt->evt_data[sizeof(*cmd_complete)];
                        bt_hci_le_cb_t callback = NULL;
                        bt_hci_get_random_context(&device, &callback);
                        if (callback) {
                            callback(device, le_rand->rand, sizeof(le_rand->rand));
                        }
                        break;
                    }
                    case BT_HCI_OP_LE_ENCRYPT:
                    {
                        struct bt_hci_rp_le_encrypt *le_encrypt =
                            (struct bt_hci_rp_le_encrypt *)&bt_hci_evt_pkt->evt_data[sizeof(*cmd_complete)];
                        bt_hci_le_cb_t callback = NULL;
                        bt_hci_get_encrypt_context(&device, &callback);
                        if (callback) {
                            callback(device, le_encrypt->enc_data, sizeof(le_encrypt->enc_data));
                        }
                        break;
                    }
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
            bt_host_get_dev_from_bdaddr(role_change->bdaddr.val, &device);
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
            bt_host_get_dev_from_bdaddr(pin_code_req->bdaddr.val, &device);
            memcpy((void *)&pin_code_reply.bdaddr, device->remote_bdaddr, sizeof(pin_code_reply.bdaddr));
            if (device->ids.type == BT_WII) {
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
            bt_host_get_dev_from_bdaddr(link_key_req->bdaddr.val, &device);
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
            bt_host_get_dev_from_bdaddr(link_key_notify->bdaddr.val, &device);
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
                    printf("# dev: %ld error: 0x%02X\n", device->ids.id, remote_ext_features->status);
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
            bt_host_get_dev_from_bdaddr(io_capa_req->bdaddr.val, &device);
            if (device) {
                bt_hci_cmd_io_capability_reply((void *)device->remote_bdaddr);
            }
            break;
        }
        case BT_HCI_EVT_USER_CONFIRM_REQ:
        {
            struct bt_hci_evt_user_confirm_req *user_confirm_req = (struct bt_hci_evt_user_confirm_req *)bt_hci_evt_pkt->evt_data;
            printf("# BT_HCI_EVT_USER_CONFIRM_REQ\n");
            bt_host_get_dev_from_bdaddr(user_confirm_req->bdaddr.val, &device);
            if (device) {
                bt_hci_cmd_user_confirm_reply((void *)device->remote_bdaddr);
            }
            break;
        }
        case BT_HCI_EVT_LE_META_EVENT:
            bt_hci_le_meta_evt_hdlr(bt_hci_evt_pkt);
            break;
    }
}
