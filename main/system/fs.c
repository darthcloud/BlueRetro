/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <esp_spiffs.h>
#include <esp_err.h>
#include "sdkconfig.h"
#include "fs.h"

#define ROOT "/fs"

int32_t fs_init(void) {
    int32_t ret;

    esp_vfs_spiffs_conf_t conf = {
      .base_path = ROOT,
      .partition_label = NULL,
      .max_files = 16,
      .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            printf("Failed to find SPIFFS partition");
        } else {
            printf("Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    esp_spiffs_info(conf.partition_label, &total, &used);

    return 0;
}

void fs_reset(void) {
    DIR *d;
    struct dirent *dir;
    d = opendir(ROOT);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            char tmp_str[32] = "/fs/";
            strcat(tmp_str, dir->d_name);
            if (strcmp(tmp_str, BDADDR_FILE) != 0 && strcmp(tmp_str, BITSTREAM_FILE) != 0) {
                if(remove(tmp_str) == 0) {
                    printf("# RM %s\n", tmp_str);
                }
            }
        }
        closedir(d);
    }
}
