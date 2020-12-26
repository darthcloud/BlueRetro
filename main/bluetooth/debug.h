/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_DBG_H_
#define _BT_DBG_H_

void bt_dbg_init(uint8_t dev_type);
void bt_dbg(uint8_t *data, uint16_t len);

#endif /* _BT_DBG_H_ */
