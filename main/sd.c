#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <esp_vfs_fat.h>
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>
#include <esp_private/esp_timer_impl.h>
#include "sd.h"
#include "io.h"

#define SD_MAGIC 0xA5A55A5A

#define ROOT "/sd"
#define CONFIG_FILE "/config.bin"


static esp_err_t sd_create_config(struct config *config) {
    FILE *config_file = fopen(ROOT CONFIG_FILE, "wb");
    if (config_file == NULL) {
        printf("failed to open file for writing\n");
        return ESP_FAIL;
    }

    config->magic = SD_MAGIC;
    config->mode = 0x00;
    config->set_map = 0x00;

    memcpy(config->mapping, map_presets, sizeof(config->mapping));

    fwrite(config, sizeof(*config), 1, config_file);
    fclose(config_file);

    return ESP_OK;
}

static esp_err_t sd_validate_config(struct config *config) {
    if (config->magic != SD_MAGIC) {
        return ESP_FAIL;
    }
    else {
        return ESP_OK;
    }
}

static esp_err_t sd_load_config(struct config *config) {
    FILE *config_file = fopen(ROOT CONFIG_FILE, "rb");
    if (config_file == NULL) {
        printf("Failed to open file for reading\n");
        return ESP_FAIL;
    }

    fread(config, sizeof(*config), 1, config_file);
    fclose(config_file);

    return ESP_OK;
}

esp_err_t sd_init(struct config *config) {
    esp_err_t ret;
    struct stat st;
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
        if (ret == ESP_FAIL) {
            printf("Failed to mount filesystem. \n"
                "If you want the card to be formatted, set format_if_mount_failed = true.\n");
        } else {
            printf("Failed to initialize the card (%s).\n"
                "Make sure SD card lines have pull-up resistors in place.\n", esp_err_to_name(ret));
        }
        return ret;
    }

    /* Card has been initialized, print its properties */
    sdmmc_card_print_info(stdout, card);

    if (stat(ROOT CONFIG_FILE, &st) != 0) {
        printf("%s: No config on SD. Creating...\n", __FUNCTION__);
        sd_create_config(config);
    }

    ret = sd_load_config(config);
    if (ret) {
        printf("%s: Failed to load config from SD.\n", __FUNCTION__);
        return ret;
    }

    while (sd_validate_config(config)) {
        printf("%s: Bad Magic\n", __FUNCTION__);
        sd_create_config(config);
    }

    return ESP_OK;
}

esp_err_t sd_update_config(struct config *config) {
    FILE *config_file = fopen(ROOT CONFIG_FILE, "wb");
    if (config_file == NULL) {
        printf("failed to open file for writing\n");
        return ESP_FAIL;
    }

    fwrite(config, sizeof(*config), 1, config_file);
    fclose(config_file);

    return ESP_OK;
}

