/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include <esp_spiffs.h>
#include <esp_err.h>
#include "sdkconfig.h"
#include "fs.h"

#define ROOT "/fs"

#ifdef CONFIG_BLUERETRO_FAT_ON_SDCARD
int32_t sd_init(void) {
    int32_t ret;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t* card;
    ret = esp_vfs_fat_sdmmc_mount(ROOT, &host, &slot_config, &mount_config, &card);

    if (ret) {
        if (ret == -1) {
            printf("Failed to mount filesystem. \n"
                "If you want the card to be formatted, set format_if_mount_failed = true.\n");
        } else {
            printf("Failed to initialize the card (%d).\n"
                "Make sure SD card lines have pull-up resistors in place.\n", ret);
        }
        return ret;
    }

    /* Card has been initialized, print its properties */
    sdmmc_card_print_info(stdout, card);

    return 0;
}
#endif

#ifdef CONFIG_BLUERETRO_FAT_ON_SPIFLASH
int32_t spiffs_init(void) {
    int32_t ret;

    esp_vfs_spiffs_conf_t conf = {
      .base_path = ROOT,
      .partition_label = NULL,
      .max_files = 5,
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
#endif

int32_t fs_init(void) {
#ifdef CONFIG_BLUERETRO_FAT_ON_SDCARD
    return sd_init();
#else
#ifdef CONFIG_BLUERETRO_FAT_ON_SPIFLASH
    return spiffs_init();
#else
#error Invalid FS config
#endif
#endif
}

void fs_reset(void) {
    (void)remove(LINK_KEYS_FILE);
    (void)remove(LE_LINK_KEYS_FILE);
    (void)remove(CONFIG_FILE);
    (void)remove(MEMORY_CARD_FILE);
}
