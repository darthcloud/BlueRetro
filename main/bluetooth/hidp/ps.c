/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <esp32/rom/crc.h>
#include <esp_timer.h>
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "ps.h"

const uint32_t bt_ps4_ps5_led_dev_id_map[] = {
    0xFF0000, /* Blue */
    0x0000FF, /* Red */
    0x00FF00, /* Green */
    0xFF00FF, /* Pink */
    0xFFFF00, /* Cyan */
    0x0080FF, /* Orange */
    0x00FFFF, /* Yellow */
    0xFF0080, /* Purple */
};

static void bt_hid_cmd_ps5_set_conf(struct bt_dev *device, void *report);

static void bt_hid_cmd_ps4_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_hci_pkt_tmp.hidp_data;

    bt_hci_pkt_tmp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_hci_pkt_tmp.hidp_hdr.protocol = BT_HIDP_PS4_SET_CONF;

    memcpy((void *)set_conf, report, sizeof(*set_conf));

    set_conf->crc = crc32_le((uint32_t)~0xFFFFFFFF, (void *)&bt_hci_pkt_tmp.hidp_hdr,
        sizeof(bt_hci_pkt_tmp.hidp_hdr) + sizeof(*set_conf) - sizeof(set_conf->crc));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_PS4_SET_CONF, sizeof(*set_conf));
}

static void bt_hid_cmd_ps5_rumble_init(struct bt_dev *device) {
    struct bt_hidp_ps5_set_conf ps5_set_conf = {
        .conf0 = 0x02,
        .cmd = 0x02,
    };

    bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_conf);
}

static void bt_hid_cmd_ps5_trigger_init(struct bt_dev *device) {
    int32_t perc_threshold_l = -1;
    int32_t perc_threshold_r = -1;
    int32_t dev = bt_host_get_dev_from_out_idx(device->ids.out_idx, &device);
    uint32_t map_cnt_l = 0;
    uint32_t map_cnt_r = 0;

    if (wired_adapter.system_id == WIRED_AUTO) {
        /* Can't configure feature if target system is unknown */
        return;
    }

    printf("# %s\n", __FUNCTION__);

    /* Make sure meta desc is init */
    adapter_meta_init();

    /* Go through the list of mappings, looking for PAD_RM and PAD_LM */
    for (uint32_t i = 0; i < config.in_cfg[dev].map_size; i++) {
        if (config.in_cfg[dev].map_cfg[i].dst_btn < BR_COMBO_BASE_1) {
            uint8_t is_axis = btn_is_axis(config.in_cfg[dev].map_cfg[i].dst_id, config.in_cfg[dev].map_cfg[i].dst_btn);
            if (config.in_cfg[dev].map_cfg[i].src_btn == PAD_RM) {
                map_cnt_r++;
                if (is_axis) {
                    continue;
                }
                if (config.in_cfg[dev].map_cfg[i].perc_threshold > perc_threshold_r) {
                    perc_threshold_r = config.in_cfg[dev].map_cfg[i].perc_threshold;
                }
            }
            else if (config.in_cfg[dev].map_cfg[i].src_btn == PAD_LM) {
                map_cnt_l++;
                if (is_axis) {
                    continue;
                }
                if (config.in_cfg[dev].map_cfg[i].perc_threshold > perc_threshold_l) {
                    perc_threshold_l = config.in_cfg[dev].map_cfg[i].perc_threshold;
                }
            }
        }
    }
    /* If only one mapping exist do not set resistance */
    if (map_cnt_r < 2) {
        perc_threshold_r = -1;
    }
    if (map_cnt_l < 2) {
        perc_threshold_l = -1;
    }

    uint8_t r2_start_resistance_value = (perc_threshold_r * 255) / 100;
    uint8_t l2_start_resistance_value = (perc_threshold_l * 255) / 100;

    uint8_t r2_trigger_start_resistance = (uint8_t)(0x94 * (r2_start_resistance_value / 255.0));
    uint8_t r2_trigger_effect_force =
        (uint8_t)((0xb4 - r2_trigger_start_resistance) * (r2_start_resistance_value / 255.0) + r2_trigger_start_resistance);

    uint8_t l2_trigger_start_resistance = (uint8_t)(0x94 * (l2_start_resistance_value / 255.0));
    uint8_t l2_trigger_effect_force =
        (uint8_t)((0xb4 - l2_trigger_start_resistance) * (l2_start_resistance_value / 255.0) + l2_trigger_start_resistance);

    struct bt_hidp_ps5_set_conf ps5_set_conf = {
        .conf0 = 0x02,
        .cmd = 0x0c,
        .r2_trigger_motor_mode = perc_threshold_r > -1 ? 0x02 : 0x00,
        .r2_trigger_start_resistance = r2_trigger_start_resistance,
        .r2_trigger_effect_force = r2_trigger_effect_force,
        .r2_trigger_range_force = 0xff,
        .r2_trigger_near_release_str = 0x00,
        .r2_trigger_near_middle_str = 0x00,
        .r2_trigger_pressed_str = 0x00,
        .r2_trigger_actuation_freq = 0x00,
        .l2_trigger_motor_mode = perc_threshold_l > -1 ? 0x02 : 0x00,
        .l2_trigger_start_resistance = l2_trigger_start_resistance,
        .l2_trigger_effect_force = l2_trigger_effect_force,
        .l2_trigger_range_force = 0xff,
        .l2_trigger_near_release_str = 0x00,
        .l2_trigger_near_middle_str = 0x00,
        .l2_trigger_pressed_str = 0x00,
        .l2_trigger_actuation_freq = 0x00,
    };

    bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_conf);
}

static void bt_hid_ps5_init_callback(void *arg) {
    struct bt_dev *device = (struct bt_dev *)arg;
    struct bt_hidp_ps5_set_conf ps5_set_conf = {
        .conf0 = 0x02,
        .conf1 = 0x08,
    };
    struct bt_hidp_ps5_set_conf ps5_set_led = {
        .conf0 = 0x02,
        .conf1 = 0xF6,
    };
    ps5_set_led.leds = bt_ps4_ps5_led_dev_id_map[device->ids.out_idx];
    printf("# %s\n", __FUNCTION__);

    bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_conf);
    bt_hid_cmd_ps5_set_conf(device, (void *)&ps5_set_led);

    /* Set trigger "click" haptic effect when rumble is on */
    if (config.out_cfg[device->ids.out_idx].acc_mode == ACC_RUMBLE
            || config.out_cfg[device->ids.out_idx].acc_mode == ACC_BOTH) {
        bt_hid_cmd_ps5_trigger_init(device);
    }

    esp_timer_delete(device->timer_hdl);
    device->timer_hdl = NULL;
}

static void bt_hid_cmd_ps5_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_ps5_set_conf *set_conf = (struct bt_hidp_ps5_set_conf *)bt_hci_pkt_tmp.hidp_data;

    /* Hack for PS5 rumble, BlueRetro design dont't allow to Q 2 frames */
    if (((struct bt_hidp_ps5_set_conf *)report)->r_rumble || ((struct bt_hidp_ps5_set_conf *)report)->l_rumble) {
        bt_hid_cmd_ps5_rumble_init(device);
    }

    bt_hci_pkt_tmp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_hci_pkt_tmp.hidp_hdr.protocol = BT_HIDP_PS5_SET_CONF;

    memcpy((void *)set_conf, report, sizeof(*set_conf));

    set_conf->crc = crc32_le((uint32_t)~0xFFFFFFFF, (void *)&bt_hci_pkt_tmp.hidp_hdr,
        sizeof(bt_hci_pkt_tmp.hidp_hdr) + sizeof(*set_conf) - sizeof(set_conf->crc));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_PS5_SET_CONF, sizeof(*set_conf));
}

void bt_hid_cmd_ps_set_conf(struct bt_dev *device, void *report) {
    switch (device->ids.subtype) {
        case BT_PS5_DS:
            bt_hid_cmd_ps5_set_conf(device, report);
            break;
        default:
            bt_hid_cmd_ps4_set_conf(device, report);
            break;
    }
}

void bt_hid_ps_init(struct bt_dev *device) {
#ifndef CONFIG_BLUERETRO_TEST_FALLBACK_REPORT
    switch (device->ids.subtype) {
        case BT_PS5_DS:
            bt_hid_ps5_init_callback((void *)device);
            break;
        default:
        {
            const esp_timer_create_args_t ps5_timer_args = {
                .callback = &bt_hid_ps5_init_callback,
                .arg = (void *)device,
                .name = "ps5_init_timer"
            };
            struct bt_hidp_ps4_set_conf ps4_set_conf = {
                .conf0 = 0xc0,
                .conf1 = 0x07,
            };
            ps4_set_conf.leds = bt_ps4_ps5_led_dev_id_map[device->ids.out_idx];

            printf("# %s\n", __FUNCTION__);

            esp_timer_create(&ps5_timer_args, (esp_timer_handle_t *)&device->timer_hdl);
            esp_timer_start_once(device->timer_hdl, 1000000);
            bt_hid_cmd_ps4_set_conf(device, (void *)&ps4_set_conf);
            break;
        }
    }
#endif
}

void bt_hid_ps_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    uint32_t hidp_data_len = len - (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
                                    + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_hidp_hdr));

    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_HID_STATUS:
                    if (device->ids.subtype != BT_SUBTYPE_DEFAULT) {
                        bt_type_update(device->ids.id, BT_PS, BT_SUBTYPE_DEFAULT);
                    }
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    break;
                case BT_HIDP_PS4_STATUS:
                    if (device->ids.report_type != BT_HIDP_PS4_STATUS) {
                        bt_type_update(device->ids.id, BT_PS, BT_SUBTYPE_DEFAULT);
                        device->ids.report_type = BT_HIDP_PS4_STATUS;
                    }
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    break;
                case BT_HIDP_PS5_STATUS:
                    if (device->ids.report_type != BT_HIDP_PS5_STATUS) {
                        bt_type_update(device->ids.id, BT_PS, BT_PS5_DS);
                        device->ids.report_type = BT_HIDP_PS5_STATUS;
                    }
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, hidp_data_len);
                    break;
            }
            break;
    }
}
