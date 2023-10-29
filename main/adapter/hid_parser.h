/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _HID_PARSER_H_
#define _HID_PARSER_H_

void hid_parser(struct bt_data *bt_data, uint8_t *data, uint32_t len);
struct hid_report *hid_parser_get_report(int32_t dev_id, uint8_t report_id);
void hid_parser_load_report(struct bt_data *bt_data, uint8_t report_id);

#endif /* _HID_PARSER_H_ */
