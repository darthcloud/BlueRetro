/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "adapter.h"

#define CONFIG_MAGIC_V0 0xA5A5A5A5
#define CONFIG_MAGIC_V1 0xF380D824
#define CONFIG_MAGIC_V2 0x1782DB8A
#define CONFIG_MAGIC_V3 0xAB89C69A
#define CONFIG_MAGIC CONFIG_MAGIC_V3
#define CONFIG_VERSION 3
#define ADAPTER_MAPPING_MAX 128

enum {
    DEFAULT_CFG = 0,
    GAMEID_CFG,
};

struct map_cfg {
    uint8_t src_btn;
    uint8_t dst_btn;
    uint8_t dst_id;
    uint8_t perc_max;
    uint8_t perc_threshold;
    uint8_t perc_deadzone;
    uint8_t turbo;
    uint8_t algo;
} __packed;

struct global_cfg {
    uint8_t system_cfg;
    uint8_t multitap_cfg;
    uint8_t inquiry_mode;
    uint8_t banksel;
} __packed;

struct out_cfg {
    uint8_t dev_mode;
    uint8_t acc_mode;
} __packed;

struct in_cfg {
    uint8_t bt_dev_id;
    uint8_t bt_subdev_id;
    uint8_t map_size;
    struct map_cfg map_cfg[ADAPTER_MAPPING_MAX];
} __packed;

struct config {
    uint32_t magic;
    struct global_cfg global_cfg;
    struct out_cfg out_cfg[WIRED_MAX_DEV];
    struct in_cfg in_cfg[WIRED_MAX_DEV];
} __packed;

extern struct config config;

void config_init(uint32_t src);
void config_update(uint32_t dst);
uint32_t config_get_src(void);

#endif /* _CONFIG_H_ */
