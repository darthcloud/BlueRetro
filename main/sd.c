#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include <esp_private/esp_timer_impl.h>

#define ROOT "/sd"
#define BTN_MAP "/btn_map.bin"

static uint8_t test[4][32];
FILE *btn_map;

void sd_init(void)
{
    struct stat st;
    uint64_t time1, time2;
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
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(ROOT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("Failed to mount filesystem. \n"
                "If you want the card to be formatted, set format_if_mount_failed = true.\n");
        } else {
            printf("Failed to initialize the card (%s).\n"
                "Make sure SD card lines have pull-up resistors in place.\n", esp_err_to_name(ret));
        }
        return;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    printf("Opening file\n");
    btn_map = fopen(ROOT BTN_MAP, "wb");
    if (btn_map == NULL) {
        printf("Failed to open file for writing\n");
        return;
    }
    time1 = esp_timer_impl_get_time();
    write(fileno(btn_map), test, 4*32);
    time2 = esp_timer_impl_get_time();
    printf("File written time: %llu\n", (time2 - time1));

    // Check if destination file exists before renaming
    if (stat("/sdcard/foo.txt", &st) == 0) {
        // Delete it if it exists
        unlink("/sdcard/foo.txt");
    }

    // All done, unmount partition and disable SDMMC or SPI peripheral
    esp_vfs_fat_sdmmc_unmount();
    printf("Card unmounted\n");
}
