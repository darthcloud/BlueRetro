#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <esp_vfs_fat.h>
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>
#include <esp_private/esp_timer_impl.h>
#include "sd.h"
#include "nsi.h"

#define SD_MAGIC 0x5A5AA5A5

#define ROOT "/sd"
#define CONFIG_FILE "/config.bin"
#define MEMPAK_FILE "/mempak.bin"
#define LINK_KEYS_FILE "/linkkeys.bin"

#define LINK_KEYS_SIZE (16 * (6 + 16 + 1) + 1)

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

esp_err_t sd_update_mempak(void) {
    FILE *mempak_file = fopen(ROOT MEMPAK_FILE, "wb");
    if (mempak_file == NULL) {
        printf("failed to open mempak file for writing\n");
        return ESP_FAIL;
    }

    fwrite(mempak, 32*1024, 1, mempak_file);
    fclose(mempak_file);

    return ESP_OK;
}

static esp_err_t sd_load_mempak(void) {
    FILE *mempak_file = fopen(ROOT MEMPAK_FILE, "rb");
    if (mempak_file == NULL) {
        printf("Failed to open mempak file for reading\n");
        return ESP_FAIL;
    }

    fread(mempak, 32 * 1024, 1, mempak_file);
    fclose(mempak_file);

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

    if (stat(ROOT MEMPAK_FILE, &st) != 0) {
        printf("%s: No mempak on SD. Creating...\n", __FUNCTION__);
        sd_update_mempak();
    }

    sd_load_mempak();

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

int32_t sd_load_link_keys(uint8_t *data, uint32_t len) {
    struct stat st;
    int32_t ret = -1;

    if (stat(ROOT LINK_KEYS_FILE, &st) != 0) {
        printf("%s: No link keys on SD. Creating...\n", __FUNCTION__);
        ret = sd_store_link_keys(data, len);
    }
    else {
        FILE *file = fopen(ROOT LINK_KEYS_FILE, "rb");
        if (file == NULL) {
            printf("%s: failed to open file for reading\n", __FUNCTION__);
        }
        else {
            fread(data, len, 1, file);
            fclose(file);
            ret = 0;
        }
    }
    return ret;
}

int32_t sd_store_link_keys(uint8_t *data, uint32_t len) {
    int32_t ret = -1;

    FILE *file = fopen(ROOT LINK_KEYS_FILE, "wb");
    if (file == NULL) {
        printf("%s: failed to open file for writing\n", __FUNCTION__);
    }
    else {
        fwrite(data, len, 1, file);
        fclose(file);
        ret = 0;
    }
    return ret;
}
