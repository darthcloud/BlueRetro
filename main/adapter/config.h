/*
 * Copyright (c) 2019-2024, Jacques Gagnon
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

struct hw_config {
    uint32_t external_adapter;
    uint32_t hotplug;
    uint32_t hw1_ports_led_pins[4];
    uint32_t led_flash_duty_cycle;
    uint32_t led_flash_hz[3];
    uint32_t led_off_duty_cycle;
    uint32_t led_on_duty_cycle;
    uint32_t led_pulse_duty_max;
    uint32_t led_pulse_duty_min;
    uint32_t led_pulse_fade_cycle_delay_ms;
    uint32_t led_pulse_fade_time_ms;
    uint32_t led_pulse_hz;
    uint32_t port_cnt;
    uint32_t ports_sense_input_polarity;
    uint32_t ports_sense_output_ms;
    uint32_t ports_sense_output_od;
    uint32_t ports_sense_output_polarity;
    uint32_t ports_sense_p3_p4_as_output;
    uint32_t power_pin_is_hold;
    uint32_t power_pin_od;
    uint32_t power_pin_polarity;
    uint32_t power_pin_pulse_ms;
    uint32_t reset_pin_od;
    uint32_t reset_pin_polarity;
    uint32_t reset_pin_pulse_ms;
    uint32_t sw_io0_hold_thres_ms[3];
    uint32_t ps_ctrl_colors[8];
    uint8_t bdaddr[6];
} __packed;

extern struct config config;
extern struct hw_config hw_config;

void config_init(uint32_t src);
void config_update(uint32_t dst);
uint32_t config_get_src(void);

#endif /* _CONFIG_H_ */
