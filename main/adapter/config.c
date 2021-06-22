/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter.h"
#include "config.h"

#define CONFIG_FILE "/fs/config.bin"

struct config config;
static uint32_t config_version_magic[] = {
    CONFIG_MAGIC_V0,
    CONFIG_MAGIC_V1,
};

static void config_init_struct(struct config *data);
static int32_t config_load_from_file(struct config *data);
static int32_t config_store_on_file(struct config *data);
static int32_t config_v0_update(struct config *data);

static int32_t (*config_ver_update[])(struct config *data) = {
    config_v0_update,
    NULL,
};

static int32_t config_v0_update(struct config *data) {
    memmove((uint8_t *)data + 7, (uint8_t *)data + 6, sizeof(*data) - 7);

    data->magic = CONFIG_MAGIC;
    data->global_cfg.inquiry_mode = INQ_AUTO;

    return config_store_on_file(data);
}

static int32_t config_get_version(uint32_t magic) {
    for (uint32_t i = 0; i < ARRAY_SIZE(config_version_magic); i++) {
        if (magic == config_version_magic[i]) {
            return i;
        }
    }
    return -1;
}

static void config_init_struct(struct config *data) {
    data->magic = CONFIG_MAGIC;
    data->global_cfg.system_cfg = WIRED_AUTO;
    data->global_cfg.multitap_cfg = MT_NONE;
    data->global_cfg.inquiry_mode = INQ_AUTO;

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        data->out_cfg[i].dev_mode = DEV_PAD;
        data->out_cfg[i].acc_mode = ACC_NONE;
        data->in_cfg[i].bt_dev_id = 0x00; /* Not used placeholder */
        data->in_cfg[i].bt_subdev_id = 0x00;  /* Not used placeholder */
        data->in_cfg[i].map_size = KBM_MAX;
        for (uint32_t j = 0; j < KBM_MAX; j++) {
            data->in_cfg[i].map_cfg[j].src_btn = j;
            data->in_cfg[i].map_cfg[j].dst_btn = j;
            data->in_cfg[i].map_cfg[j].dst_id = i;
            data->in_cfg[i].map_cfg[j].perc_max = 100;
            data->in_cfg[i].map_cfg[j].perc_threshold = 50;
            data->in_cfg[i].map_cfg[j].perc_deadzone = 135;
            data->in_cfg[i].map_cfg[j].turbo = 0;
            data->in_cfg[i].map_cfg[j].algo = 0;
        }
    }
}

static int32_t config_load_from_file(struct config *data) {
    struct stat st;
    int32_t ret = -1;

    if (stat(CONFIG_FILE, &st) != 0) {
        printf("%s: No config on FATFS. Creating...\n", __FUNCTION__);
        config_init_struct(data);
        ret = config_store_on_file(data);
    }
    else {
        FILE *file = fopen(CONFIG_FILE, "rb");
        if (file == NULL) {
            printf("%s: failed to open file for reading\n", __FUNCTION__);
        }
        else {
            uint32_t count = fread((void *)data, sizeof(*data), 1, file);
            fclose(file);
            if (count == 1) {
                ret = 0;
            }
        }
    }

    if (data->magic != CONFIG_MAGIC) {
        int32_t file_ver = config_get_version(data->magic);
        if (file_ver == -1) {
            printf("%s: Bad magic, reset config\n", __FUNCTION__);
            config_init_struct(data);
            ret = config_store_on_file(data);
        }
        else {
            printf("%s: Upgrading cfg v%d to v%d\n", __FUNCTION__, file_ver, CONFIG_VERSION);
            for (uint32_t i = file_ver; i < CONFIG_VERSION; i++) {
                if (config_ver_update[i]) {
                    ret = config_ver_update[i](data);
                }
            }
        }
    }

    return ret;
}

static int32_t config_store_on_file(struct config *data) {
    int32_t ret = -1;

    FILE *file = fopen(CONFIG_FILE, "wb");
    if (file == NULL) {
        printf("%s: failed to open file for writing\n", __FUNCTION__);
    }
    else {
        fwrite((void *)data, sizeof(*data), 1, file);
        fclose(file);
        ret = 0;
    }
    return ret;
}

void config_init(void) {
    config_load_from_file(&config);
}

void config_update(void) {
    config_store_on_file(&config);
}
