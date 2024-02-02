/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "nvs.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter.h"
#include "config.h"
#include "system/fs.h"
#include "adapter/gameid.h"

struct config config;
struct hw_config hw_config = {
    .external_adapter = 0,
    .hotplug = 0,
    .hw1_ports_led_pins = {2, 4, 12, 15},
    .led_flash_duty_cycle = 0x80000,
    .led_flash_hz = {2, 4, 8},
    .led_off_duty_cycle = 0,
    .led_on_duty_cycle = 0xFFFFF,
    .led_pulse_duty_max = 2000,
    .led_pulse_duty_min = 50,
    .led_pulse_fade_cycle_delay_ms = 500,
    .led_pulse_fade_time_ms = 500,
    .led_pulse_hz = 5000,
    .port_cnt = 2,
    .ports_sense_input_polarity = 0,
    .ports_sense_output_ms = 1000,
    .ports_sense_output_od = 0,
    .ports_sense_output_polarity = 0,
    .ports_sense_p3_p4_as_output = 0,
    .power_pin_is_hold = 0,
    .power_pin_od = 0,
    .power_pin_polarity = 0,
    .power_pin_pulse_ms = 20,
    .reset_pin_od = 1,
    .reset_pin_polarity = 0,
    .reset_pin_pulse_ms = 500,
    .sw_io0_hold_thres_ms = {1000, 3000, 6000},
    .ps_ctrl_colors = {
        0xFF0000, /* Blue */
        0x0000FF, /* Red */
        0x00FF00, /* Green */
        0xFF00FF, /* Pink */
        0xFFFF00, /* Cyan */
        0x0080FF, /* Orange */
        0x00FFFF, /* Yellow */
        0xFF0080, /* Purple */
    },
};

static char *hw_config_name_idx[] = {
    "ext_adapter",
    "hotplug",
    "hw1_led_pins_0",
    "hw1_led_pins_1",
    "hw1_led_pins_2",
    "hw1_led_pins_3",
    "led_flash_duty",
    "led_flash_hz_0",
    "led_flash_hz_1",
    "led_flash_hz_2",
    "led_off_duty",
    "led_on_duty",
    "led_p_duty_max",
    "led_p_duty_min",
    "led_p_fade_c_ms",
    "led_p_fade_t_ms",
    "led_pulse_hz",
    "port_cnt",
    "ports_s_in_pol",
    "ports_s_out_ms",
    "ports_s_out_od",
    "ports_s_out_pol",
    "ports_s_out_en",
    "pwr_pin_is_hold",
    "pwr_pin_od",
    "pwr_pin_pol",
    "pwr_pin_p_ms",
    "reset_pin_od",
    "reset_pin_pol",
    "reset_pin_p_ms",
    "sw_thres_ms_0",
    "sw_thres_ms_1",
    "sw_thres_ms_2",
    "ps_ctrl_color_0",
    "ps_ctrl_color_1",
    "ps_ctrl_color_2",
    "ps_ctrl_color_3",
    "ps_ctrl_color_4",
    "ps_ctrl_color_5",
    "ps_ctrl_color_6",
    "ps_ctrl_color_7",
};
static uint32_t config_src = DEFAULT_CFG;
static uint32_t config_version_magic[] = {
    CONFIG_MAGIC_V0,
    CONFIG_MAGIC_V1,
    CONFIG_MAGIC_V2,
    CONFIG_MAGIC_V3,
};
static uint8_t config_default_combo[BR_COMBO_CNT] = {
    PAD_LM, PAD_RM, PAD_MM, PAD_RB_UP, PAD_RB_LEFT, PAD_RB_RIGHT, PAD_RB_DOWN, PAD_LD_UP, PAD_LD_DOWN, PAD_MS
};

static void config_init_struct(struct config *data);
static void config_init_nvs_patch(struct config *data);
static int32_t config_load_from_file(struct config *data, char *filename);
static int32_t config_store_on_file(struct config *data, char *filename);
static int32_t config_v0_update(struct config *data, char *filename);
static int32_t config_v1_update(struct config *data, char *filename);
static int32_t config_v2_update(struct config *data, char *filename);

static int32_t (*config_ver_update[])(struct config *data, char *filename) = {
    config_v0_update,
    config_v1_update,
    config_v2_update,
    NULL,
};

static int32_t config_v0_update(struct config *data, char *filename) {
    memmove((uint8_t *)data + 7, (uint8_t *)data + 6, sizeof(*data) - 7);

    data->magic = CONFIG_MAGIC;
    data->global_cfg.inquiry_mode = INQ_AUTO;

    return config_store_on_file(data, filename);
}

static int32_t config_v1_update(struct config *data, char *filename) {
    memmove((uint8_t *)data + 8, (uint8_t *)data + 7, 31 - 8);

    data->magic = CONFIG_MAGIC;
    data->global_cfg.banksel = 0;

    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("%s: failed to open file for reading\n", __FUNCTION__);
        goto fail;
    }
    else {
        uint32_t count = 0;
        for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
            fseek(file, 31 + (3 + 255 * 8) * i, SEEK_SET);
            count += fread((uint8_t *)&data->in_cfg[i], sizeof(struct in_cfg), 1, file);
            if (data->in_cfg[i].map_size > ADAPTER_MAPPING_MAX) {
                data->in_cfg[i].map_size = ADAPTER_MAPPING_MAX;
            }
        }
        fclose(file);

        if (count != WIRED_MAX_DEV) {
            goto fail;
        }
    }
    return config_store_on_file(data, filename);

fail:
    printf("%s: Update failed, reset config (Sorry!)\n", __FUNCTION__);
    config_init_struct(data);
    config_init_nvs_patch(data);
    return config_store_on_file(data, filename);
}

static int32_t config_get_version(uint32_t magic) {
    for (uint32_t i = 0; i < ARRAY_SIZE(config_version_magic); i++) {
        if (magic == config_version_magic[i]) {
            return i;
        }
    }
    return -1;
}

static int32_t config_v2_update(struct config *data, char *filename) {
    data->magic = CONFIG_MAGIC;

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        uint32_t j = data->in_cfg[i].map_size;
        data->in_cfg[i].map_size += BR_COMBO_CNT;
        for (uint32_t k = 0; k < BR_COMBO_CNT; j++, k++) {
            data->in_cfg[i].map_cfg[j].src_btn = config_default_combo[k];
            data->in_cfg[i].map_cfg[j].dst_btn = k + BR_COMBO_BASE_1;
            data->in_cfg[i].map_cfg[j].dst_id = i;
            data->in_cfg[i].map_cfg[j].perc_max = 100;
            data->in_cfg[i].map_cfg[j].perc_threshold = 50;
            data->in_cfg[i].map_cfg[j].perc_deadzone = 135;
            data->in_cfg[i].map_cfg[j].turbo = 0;
            data->in_cfg[i].map_cfg[j].algo = 0;
        }
    }

    return config_store_on_file(data, filename);
}

static int32_t hw_config_lookup_key_name(const char* key) {
    for (uint32_t i = 0; i < sizeof(hw_config_name_idx)/sizeof(*hw_config_name_idx); i++) {
        if (strstr(key, hw_config_name_idx[i]) != NULL) {
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
    data->global_cfg.banksel = 0;

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        data->out_cfg[i].dev_mode = DEV_PAD;
        data->out_cfg[i].acc_mode = ACC_NONE;
        data->in_cfg[i].bt_dev_id = 0x00; /* Not used placeholder */
        data->in_cfg[i].bt_subdev_id = 0x00;  /* Not used placeholder */
        data->in_cfg[i].map_size = KBM_MAX + BR_COMBO_CNT;
        uint32_t j = 0;
        for (; j < KBM_MAX; j++) {
            data->in_cfg[i].map_cfg[j].src_btn = j;
            data->in_cfg[i].map_cfg[j].dst_btn = j;
            data->in_cfg[i].map_cfg[j].dst_id = i;
            data->in_cfg[i].map_cfg[j].perc_max = 100;
            data->in_cfg[i].map_cfg[j].perc_threshold = 50;
            data->in_cfg[i].map_cfg[j].perc_deadzone = 135;
            data->in_cfg[i].map_cfg[j].turbo = 0;
            data->in_cfg[i].map_cfg[j].algo = 0;
        }
        for (uint32_t k = 0; k < BR_COMBO_CNT; j++, k++) {
            data->in_cfg[i].map_cfg[j].src_btn = config_default_combo[k];
            data->in_cfg[i].map_cfg[j].dst_btn = k + BR_COMBO_BASE_1;
            data->in_cfg[i].map_cfg[j].dst_id = i;
            data->in_cfg[i].map_cfg[j].perc_max = 100;
            data->in_cfg[i].map_cfg[j].perc_threshold = 50;
            data->in_cfg[i].map_cfg[j].perc_deadzone = 135;
            data->in_cfg[i].map_cfg[j].turbo = 0;
            data->in_cfg[i].map_cfg[j].algo = 0;
        }
    }
}

static void config_init_nvs_patch(struct config *data) {
    esp_err_t err;
    nvs_handle_t nvs;
    uint8_t value;

    err = nvs_open("global", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        err = nvs_get_u8(nvs, "system", &value);
        if (err == ESP_OK) {
            data->global_cfg.system_cfg = value;
        }
        err = nvs_get_u8(nvs, "multitap", &value);
        if (err == ESP_OK) {
            data->global_cfg.multitap_cfg = value;
        }
        err = nvs_get_u8(nvs, "inquiry", &value);
        if (err == ESP_OK) {
            data->global_cfg.inquiry_mode = value;
        }
        err = nvs_get_u8(nvs, "bank", &value);
        if (err == ESP_OK) {
            data->global_cfg.banksel = value;
        }
        nvs_close(nvs);
    }

    err = nvs_open("output", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        err = nvs_get_u8(nvs, "mode", &value);
        if (err == ESP_OK) {
            for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
                data->out_cfg[i].dev_mode = value;
            }
        }
        err = nvs_get_u8(nvs, "accessories", &value);
        if (err == ESP_OK) {
            for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
                data->out_cfg[i].acc_mode = value;
            }
        }
        nvs_close(nvs);
    }

    err = nvs_open("mapping", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        nvs_iterator_t it = NULL;
        err = nvs_entry_find_in_handle(nvs, NVS_TYPE_ANY, &it);
        while (err == ESP_OK) {
            nvs_entry_info_t info;
            nvs_entry_info(it, &info);
            errno = 0;
            uint32_t index = strtol(info.key, NULL, 10);
            if (!errno) {
                struct map_cfg mapping = {0};
                size_t size = sizeof(struct map_cfg);
                err = nvs_get_blob(nvs, info.key, &mapping, &size);
                if (err == ESP_OK) {
                    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
                        memcpy(&data->in_cfg[i].map_cfg[index], &mapping, sizeof(struct map_cfg));
                    }
                }
            }
            err = nvs_entry_next(&it);
        }
        nvs_release_iterator(it);
        nvs_close(nvs);
    }
}

static int32_t config_load_from_file(struct config *data, char *filename) {
#ifdef CONFIG_BLUERETRO_QEMU
    config_init_struct(data);
    config_init_nvs_patch(data);
    return 0;
#else
    struct stat st;
    int32_t ret = -1;

    if (stat(filename, &st) != 0) {
        printf("%s: No config on FS. Creating...\n", __FUNCTION__);
        config_init_struct(data);
        config_init_nvs_patch(data);
        ret = config_store_on_file(data, filename);
    }
    else {
        FILE *file = fopen(filename, "rb");
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
            ret = config_store_on_file(data, filename);
        }
        else {
            printf("%s: Upgrading cfg v%ld to v%d\n", __FUNCTION__, file_ver, CONFIG_VERSION);
            for (uint32_t i = file_ver; i < CONFIG_VERSION; i++) {
                if (config_ver_update[i]) {
                    ret = config_ver_update[i](data, filename);
                }
            }
        }
    }

    return ret;
#endif /* CONFIG_BLUERETRO_QEMU */
}

static int32_t config_store_on_file(struct config *data, char *filename) {
    int32_t ret = -1;

    FILE *file = fopen(filename, "wb");
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

void hw_config_patch(void) {
    esp_err_t err;
    nvs_handle_t nvs;

    err = nvs_open("hw", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        nvs_iterator_t it = NULL;
        err = nvs_entry_find_in_handle(nvs, NVS_TYPE_ANY, &it);
        while (err == ESP_OK) {
            nvs_entry_info_t info;
            nvs_entry_info(it, &info);
            int32_t index = hw_config_lookup_key_name(info.key);
            if (index > -1) {
                uint32_t value;
                err = nvs_get_u32(nvs, info.key, &value);
                if (err == ESP_OK) {
                    hw_config.data32[index] = value;
                }
            }
            err = nvs_entry_next(&it);
        }
        nvs_release_iterator(it);
        nvs_close(nvs);
    }
}

void config_init(uint32_t src) {
    char tmp_str[32] = "/fs/";
    char *filename = CONFIG_FILE;
    char *gameid = gid_get();
    config_src = DEFAULT_CFG;

    if (src == GAMEID_CFG && strlen(gameid)) {
        struct stat st;

        strcat(tmp_str, gameid);
        if (stat(tmp_str, &st) == 0) {
            filename = tmp_str;
            config_src = GAMEID_CFG;
        }
    }

    config_load_from_file(&config, filename);
}

void config_update(uint32_t dst) {
    char tmp_str[32] = "/fs/";
    char *filename = CONFIG_FILE;
    char *gameid = gid_get();
    config_src = DEFAULT_CFG;

    if (dst == GAMEID_CFG && strlen(gameid)) {
        strcat(tmp_str, gameid);
        filename = tmp_str;
        config_src = GAMEID_CFG;
    }

    config_store_on_file(&config, filename);
}

uint32_t config_get_src(void) {
    return config_src;
}
