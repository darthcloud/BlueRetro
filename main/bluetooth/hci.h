/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HCI_H_
#define _BT_HCI_H_

typedef void (*bt_hci_le_cb_t)(struct bt_dev *device, uint8_t *data, uint32_t len);

int32_t bt_hci_init(void);
const char *bt_hci_get_device_name(void);
void bt_hci_start_inquiry(void);
void bt_hci_stop_inquiry(void);
uint32_t bt_hci_get_inquiry(void);
void bt_hci_inquiry_override(uint32_t state);
void bt_hci_disconnect(struct bt_dev *device);
void bt_hci_exit_sniff_mode(struct bt_dev *device);
void bt_hci_get_le_local_addr(bt_addr_le_t *le_local);
int32_t bt_hci_get_random(struct bt_dev *device, bt_hci_le_cb_t cb);
int32_t bt_hci_get_encrypt(struct bt_dev *device, bt_hci_le_cb_t cb, const uint8_t *key, uint8_t *plaintext);
void bt_hci_start_encryption(uint16_t handle, uint64_t rand, uint16_t ediv, uint8_t *ltk);
void bt_hci_add_to_accept_list(bt_addr_le_t *le_bdaddr);
void bt_hci_le_conn_update(struct hci_cp_le_conn_update *cp);
void bt_hci_evt_hdlr(struct bt_hci_pkt *bt_hci_evt_pkt);

#endif /* _BT_HCI_H_ */
