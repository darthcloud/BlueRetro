/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _FS_H_
#define _FS_H_

#define LINK_KEYS_FILE "/fs/linkkeys.bin"
#define LE_LINK_KEYS_FILE "/fs/le_linkkeys.bin"
#define BDADDR_FILE "/fs/bdaddr.bin"
#define CONFIG_FILE "/fs/config.bin"
#define MEMORY_CARD_FILE "/fs/mc.bin"
#define BITSTREAM_FILE "/fs/bitstream.bit"

int32_t fs_init(void);
void fs_reset(void);

#endif /* _FS_H_ */
