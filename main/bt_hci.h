#ifndef _BT_HCI_H_
#define _BT_HCI_H_

void bt_hci_cmd_inquiry(void *cp);
void bt_hci_cmd_inquiry_cancel(void *cp);
void bt_hci_cmd_connect(bt_addr_t *bdaddr);
void bt_hci_cmd_accept_conn_req(bt_addr_t *bdaddr);
void bt_hci_cmd_link_key_neg_reply(bt_addr_t *bdaddr);
void bt_hci_cmd_pin_code_reply(bt_addr_t bdaddr, uint8_t pin_len, uint8_t *pin_code);
void bt_hci_cmd_pin_code_neg_reply(bt_addr_t bdaddr);
void bt_hci_cmd_auth_requested(uint16_t handle);
void bt_hci_cmd_set_conn_encrypt(uint16_t handle);
void bt_hci_cmd_remote_name_request(bt_addr_t bdaddr);
void bt_hci_cmd_read_remote_features(uint16_t handle);
void bt_hci_cmd_read_remote_ext_features(uint16_t handle);
void bt_hci_cmd_io_capability_reply(bt_addr_t *bdaddr);
void bt_hci_cmd_user_confirm_reply(bt_addr_t *bdaddr);
void bt_hci_cmd_switch_role(bt_addr_t *bdaddr, uint8_t role);
void bt_hci_cmd_read_link_policy(uint16_t handle);
void bt_hci_cmd_write_link_policy(uint16_t handle, uint16_t link_policy);
void bt_hci_cmd_read_default_link_policy(void *cp);
void bt_hci_cmd_write_default_link_policy(void *cp);
void bt_hci_cmd_set_event_mask(void *cp);
void bt_hci_cmd_reset(void *cp);
void bt_hci_cmd_set_event_filter(void *cp);
void bt_hci_cmd_read_stored_link_key(void *cp);
void bt_hci_cmd_delete_stored_link_key(void *cp);
void bt_hci_cmd_write_local_name(void *cp);
void bt_hci_cmd_read_local_name(void *cp);
void bt_hci_cmd_write_conn_accept_timeout(void *cp);
void bt_hci_cmd_write_page_scan_timeout(void *cp);
void bt_hci_cmd_write_scan_enable(void *cp);
void bt_hci_cmd_write_page_scan_activity(void *cp);
void bt_hci_cmd_write_inquiry_scan_activity(void *cp);
void bt_hci_cmd_write_auth_enable(void *cp);
void bt_hci_cmd_read_page_scan_activity(void *cp);
void bt_hci_cmd_read_class_of_device(void *cp);
void bt_hci_cmd_write_class_of_device(void *cp);
void bt_hci_cmd_read_voice_setting(void *cp);
void bt_hci_cmd_write_hold_mode_act(void *cp);
void bt_hci_cmd_read_num_supported_iac(void *cp);
void bt_hci_cmd_read_current_iac_lap(void *cp);
void bt_hci_cmd_write_inquiry_mode(void *cp);
void bt_hci_cmd_read_page_scan_type(void *cp);
void bt_hci_cmd_write_page_scan_type(void *cp);
void bt_hci_cmd_write_ssp_mode(void *cp);
void bt_hci_cmd_read_inquiry_rsp_tx_pwr_lvl(void *cp);
void bt_hci_cmd_write_le_host_supp(void *cp);
void bt_hci_cmd_read_local_version_info(void *cp);
void bt_hci_cmd_read_supported_commands(void *cp);
void bt_hci_cmd_read_local_features(void *cp);
void bt_hci_cmd_read_local_ext_features(void *cp);
void bt_hci_cmd_read_buffer_size(void *cp);
void bt_hci_cmd_read_bd_addr(void *cp);
void bt_hci_cmd_read_data_block_size(void *cp);
void bt_hci_cmd_read_local_codecs(void *cp);
void bt_hci_cmd_read_local_sp_options(void *cp);

#endif /* _BT_HCI_H_ */
