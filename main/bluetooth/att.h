/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_ATT_H_
#define _BT_ATT_H_

#include <stdint.h>

void bt_att_cmd(uint16_t handle, uint8_t code, uint16_t len);
void bt_att_cmd_error_rsp(uint16_t handle, uint8_t req_opcode, uint16_t err_handle, uint8_t err);
void bt_att_cmd_mtu_req(uint16_t handle, uint16_t mtu);
void bt_att_cmd_mtu_rsp(uint16_t handle, uint16_t mtu);
void bt_att_cmd_find_info_req(uint16_t handle, uint16_t start, uint16_t end);
void bt_att_cmd_read_type_req_uuid16(uint16_t handle, uint16_t start, uint16_t end, uint16_t uuid);
void bt_att_cmd_read_req(uint16_t handle, uint16_t att_handle);
void bt_att_cmd_read_blob_req(uint16_t handle, uint16_t att_handle, uint16_t offset);
void bt_att_cmd_read_group_req_uuid16(uint16_t handle, uint16_t start, uint16_t uuid);
void bt_att_cmd_write_req(uint16_t handle, uint16_t att_handle, uint8_t *data, uint32_t len);
void bt_att_cmd_write_cmd(uint16_t handle, uint16_t att_handle, uint8_t *data, uint32_t len);
void bt_att_cmd_wr_rsp(uint16_t handle);
void bt_att_cmd_prep_wr_rsp(uint16_t handle, uint8_t *data, uint32_t data_len);
void bt_att_cmd_exec_wr_rsp(uint16_t handle);
void bt_att_set_le_max_mtu(uint16_t le_max_mtu);
uint16_t bt_att_get_le_max_mtu(void);

#endif /* _BT_ATT_H_ */
