#include <stdio.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include "sd.h"

#define ROOT "/sd"

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
