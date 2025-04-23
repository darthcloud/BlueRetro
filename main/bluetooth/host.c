/*
 * Copyright (c) 2019-2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <sys/stat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <esp_system.h>
#include <esp_bt.h>
#include <esp_mac.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include "nvs.h"
#include "queue_bss.h"
#include "host.h"
#include "hci.h"
#include "l2cap.h"
#include "sdp.h"
#include "att_cfg.h"
#include "att_hid.h"
#include "smp.h"
#include "tools/util.h"
#include "debug.h"
#include "mon.h"
#include "system/fs.h"
#include "adapter/config.h"
#include "adapter/gameid.h"
#include "adapter/memory_card.h"
#include "adapter/wired/wired.h"
#include "adapter/hid_parser.h"
#include "bluetooth/hidp/ps.h"

#define BT_TX 0
#define BT_RX 1
#define BT_FB_TASK_DELAY_CNT 30

enum {
    /* BT CTRL flags */
    BT_CTRL_READY = 0,
    BT_HOST_DBG_MODE,
};

struct bt_host_link_keys {
    uint32_t index;
    struct bt_hci_evt_link_key_notify link_keys[16];
} __packed;

struct bt_host_le_link_keys {
    uint32_t index;
    struct {
        bt_addr_le_t le_bdaddr;
        struct bt_smp_encrypt_info ltk;
        struct bt_smp_master_ident ident;
    } keys[16];
} __packed;

struct bt_hci_pkt bt_hci_pkt_tmp;

static struct bt_host_link_keys bt_host_link_keys = {0};
static struct bt_host_le_link_keys bt_host_le_link_keys = {0};
static RingbufHandle_t txq_hdl;
static struct bt_dev bt_dev_conf = {0};
static struct bt_dev bt_dev[BT_MAX_DEV] = {0};
static atomic_t bt_flags = 0;
static uint32_t frag_size = 0;
static uint32_t frag_offset = 0;
static uint8_t frag_buf[1024];

static int32_t bt_host_load_bdaddr_from_nvs(void);
static int32_t bt_host_load_keys_from_file(struct bt_host_link_keys *data);
static int32_t bt_host_store_keys_on_file(struct bt_host_link_keys *data);
static int32_t bt_host_load_le_keys_from_file(struct bt_host_le_link_keys *data);
static int32_t bt_host_store_le_keys_on_file(struct bt_host_le_link_keys *data);
static void bt_host_acl_hdlr(struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);
static void bt_host_tx_pkt_ready(void);
static int bt_host_rx_pkt(uint8_t *data, uint16_t len);
static void bt_host_task(void *param);

static esp_vhci_host_callback_t vhci_host_cb = {
    bt_host_tx_pkt_ready,
    bt_host_rx_pkt
};

static int32_t bt_host_load_bdaddr_from_nvs(void) {
    esp_err_t err;
    nvs_handle_t nvs;
    int32_t ret = -1;

    err = nvs_open("hw", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        uint8_t test_mac[6];
        size_t size = sizeof(test_mac);
        err = nvs_get_blob(nvs, "bdaddr", test_mac, &size);
        if (err == ESP_OK) {
            test_mac[5] -= 2; /* Set base mac to BDADDR-2 so that BDADDR end up what we want */
            esp_base_mac_addr_set(test_mac);
            printf("# %s: Using NVS MAC\n", __FUNCTION__);
            ret = 0;
        }
        nvs_close(nvs);
    }
    return ret;
}

static int32_t bt_host_load_keys_from_file(struct bt_host_link_keys *data) {
    struct stat st;
    int32_t ret = -1;

    if (stat(LINK_KEYS_FILE, &st) != 0) {
        printf("# %s: No link keys on SD. Creating...\n", __FUNCTION__);
        ret = bt_host_store_keys_on_file(data);
    }
    else {
        FILE *file = fopen(LINK_KEYS_FILE, "rb");
        if (file == NULL) {
            printf("# %s: failed to open file for reading\n", __FUNCTION__);
        }
        else {
            uint32_t count = fread((void *)data, sizeof(*data), 1, file);
            fclose(file);
            if (count == 1) {
                ret = 0;
            }
        }
    }
    return ret;
}

static int32_t bt_host_store_keys_on_file(struct bt_host_link_keys *data) {
    int32_t ret = -1;

    FILE *file = fopen(LINK_KEYS_FILE, "wb");
    if (file == NULL) {
        printf("# %s: failed to open file for writing\n", __FUNCTION__);
    }
    else {
        fwrite((void *)data, sizeof(*data), 1, file);
        fclose(file);
        ret = 0;
    }
    return ret;
}

static int32_t bt_host_load_le_keys_from_file(struct bt_host_le_link_keys *data) {
    struct stat st;
    int32_t ret = -1;

    if (stat(LE_LINK_KEYS_FILE, &st) != 0) {
        printf("# %s: No link keys on SD. Creating...\n", __FUNCTION__);
        ret = bt_host_store_le_keys_on_file(data);
    }
    else {
        FILE *file = fopen(LE_LINK_KEYS_FILE, "rb");
        if (file == NULL) {
            printf("# %s: failed to open file for reading\n", __FUNCTION__);
        }
        else {
            uint32_t count = fread((void *)data, sizeof(*data), 1, file);
            fclose(file);
            if (count == 1) {
                ret = 0;
            }
        }
    }
    return ret;
}

static int32_t bt_host_store_le_keys_on_file(struct bt_host_le_link_keys *data) {
    int32_t ret = -1;

    FILE *file = fopen(LE_LINK_KEYS_FILE, "wb");
    if (file == NULL) {
        printf("# %s: failed to open file for writing\n", __FUNCTION__);
    }
    else {
        fwrite((void *)data, sizeof(*data), 1, file);
        fclose(file);
        ret = 0;
    }
    return ret;
}

static void bt_tx_task(void *param) {
    size_t packet_len;
    uint8_t *packet;

    while(1) {
        /* TX packet from Q */
        if (atomic_test_bit(&bt_flags, BT_CTRL_READY)) {
            packet = (uint8_t *)xRingbufferReceive(txq_hdl, &packet_len, portMAX_DELAY);
            if (packet) {
                if (packet[0] == 0xFF) {
                    /* Internal wait packet */
                    vTaskDelay(packet[1] / portTICK_PERIOD_MS);
                }
                else {
                    bt_mon_tx((packet[0] == BT_HCI_H4_TYPE_CMD) ? BT_MON_CMD : BT_MON_ACL_TX,
                        packet + 1, packet_len - 1);
                    atomic_clear_bit(&bt_flags, BT_CTRL_READY);
                    esp_vhci_host_send_packet(packet, packet_len);
                }
                vRingbufferReturnItem(txq_hdl, (void *)packet);
            }
        }
        else {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
}

static void bt_fb_task(void *param) {
    static bool rumble_en = false;
    static bool rumble_on = false;
    uint32_t *fb_len;
    struct raw_fb *fb_data = NULL;
    uint32_t delay_cnt = BT_FB_TASK_DELAY_CNT; /* 100ms * 30 = 3sec */

    while(1) {
        /* Look for rumble/led feedback data */
        while ((fb_data = (struct raw_fb *)queue_bss_dequeue(wired_adapter.input_q_hdl, &fb_len))) {
            struct bt_dev *device = NULL;
            struct bt_data *bt_data = NULL;

            bt_host_get_active_dev_from_out_idx(fb_data->header.wired_id, &device);
            if (device) {
                bt_data = &bt_adapter.data[device->ids.id];
            }

            switch (fb_data->header.type) {
                case FB_TYPE_MEM_WRITE:
                    mc_storage_update();
                    break;
                case FB_TYPE_STATUS_LED:
                case FB_TYPE_PLAYER_LED:
                    if (device && device->ids.subtype == BT_PS5_DS) {
                        /* We need to clear LEDs before setting them */
                        struct bt_hidp_ps5_set_conf ps5_clear_led = {
                            .conf0 = 0x02,
                            .conf1 = 0x08,
                        };
                        bt_hid_cmd_ps_set_conf(device, (void *)&ps5_clear_led);
                    }
                    /* Fallthrough */
                case FB_TYPE_RUMBLE:
                    if (bt_data) {
                        rumble_en = true;
                        rumble_on = (bool)adapter_bridge_fb(fb_data, bt_data);
                        delay_cnt = 0;
                    }
                    break;
                case FB_TYPE_GAME_ID:
                    if (gid_update(fb_data)) {
                        config_init(GAMEID_CFG);
                    }
                    break;
                case FB_TYPE_SYS_ID:
                    if (gid_update_sys(fb_data)) {
                        config_init(GAMEID_CFG);
                    }
                    break;
                default:
                    break;
            }
            queue_bss_return(wired_adapter.input_q_hdl, (uint8_t *)fb_data, fb_len);
        }

        /* TX Feedback every 100 ms if rumble on, every 3 sec otherwise */
        if (delay_cnt-- == 0 || rumble_on) {
            for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
                struct bt_dev *device = &bt_dev[i];

                if (rumble_en && atomic_test_bit(&device->flags, BT_DEV_HID_INIT_DONE)) {
                    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];

                    bt_hid_feedback(device, bt_data->base.output);
                }
            }
            delay_cnt = BT_FB_TASK_DELAY_CNT;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

static void bt_host_task(void *param) {
    while (1) {
        /* Per device housekeeping */
        for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
            struct bt_dev *device = &bt_dev[i];
            struct bt_data *bt_data = &bt_adapter.data[i];

            /* Parse SDP data if available */
            if (atomic_test_bit(&device->flags, BT_DEV_DEVICE_FOUND)) {
                if (atomic_test_bit(&device->flags, BT_DEV_SDP_DATA)) {
                    int32_t old_type = device->ids.type;

                    bt_sdp_parser(bt_data);
                    if (old_type != device->ids.type) {
                        if (atomic_test_bit(&device->flags, BT_DEV_HID_INTR_READY)) {
                            bt_hid_init(device);
                        }
                    }
                    atomic_clear_bit(&device->flags, BT_DEV_SDP_DATA);
                }
            }
        }

        /* Update turbo mask for parallel system */
        wired_para_turbo_mask_hdlr();

#ifdef CONFIG_BLUERETRO_ADAPTER_RUMBLE_DBG
    adapter_toggle_fb(0, 150000,
        wired_adapter.data[0].output[16], wired_adapter.data[0].output[17]);
#endif

        vTaskDelay(16 / portTICK_PERIOD_MS);
    }
}

static void bt_host_acl_hdlr(struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    struct bt_dev *device = NULL;
    struct bt_hci_pkt *pkt = bt_hci_acl_pkt;
    uint32_t pkt_len = len;
    bt_host_get_dev_from_handle(pkt->acl_hdr.handle, &device);

    if (bt_acl_flags(pkt->acl_hdr.handle) == BT_ACL_CONT) {
        memcpy(frag_buf + frag_offset, (void *)pkt + BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE,
            pkt->acl_hdr.len);
        frag_offset += pkt->acl_hdr.len;
        if (frag_offset < frag_size) {
            //printf("# %s Waiting for next fragment. offset: %ld size %ld\n", __FUNCTION__, frag_offset, frag_size);
            return;
        }
        pkt = (struct bt_hci_pkt *)frag_buf;
        pkt_len = frag_size;
        //printf("# %s process reassembled frame. offset: %ld size %ld\n", __FUNCTION__, frag_offset, frag_size);
    }
    if (bt_acl_flags(pkt->acl_hdr.handle) == BT_ACL_START
        && (pkt_len - (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE + sizeof(struct bt_l2cap_hdr))) < pkt->l2cap_hdr.len) {
        memcpy(frag_buf, (void *)pkt, pkt_len);
        frag_offset = pkt_len;
        frag_size = pkt->l2cap_hdr.len + BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE + sizeof(struct bt_l2cap_hdr);
        //printf("# %s Detected fragmented frame start\n", __FUNCTION__);
        return;
    }

    if (device == NULL) {
        if (pkt->l2cap_hdr.cid == BT_L2CAP_CID_ATT) {
            bt_att_cfg_hdlr(&bt_dev_conf, pkt, pkt_len);
        }
        else {
            printf("# %s dev NULL!\n", __FUNCTION__);
        }
        return;
    }

    if (pkt->l2cap_hdr.cid == BT_L2CAP_CID_BR_SIG) {
        bt_l2cap_sig_hdlr(device, pkt);
    }
    else if (pkt->l2cap_hdr.cid == BT_L2CAP_CID_ATT) {
        bt_att_hid_hdlr(device, pkt, pkt_len);
    }
    else if (pkt->l2cap_hdr.cid == BT_L2CAP_CID_LE_SIG) {
        bt_l2cap_le_sig_hdlr(device, pkt, pkt_len);
    }
    else if (pkt->l2cap_hdr.cid == BT_L2CAP_CID_SMP) {
        bt_smp_hdlr(device, pkt, pkt_len);
    }
    else if (pkt->l2cap_hdr.cid == device->sdp_tx_chan.scid ||
        pkt->l2cap_hdr.cid == device->sdp_rx_chan.scid) {
        bt_sdp_hdlr(device, pkt);
    }
    else if (pkt->l2cap_hdr.cid == device->ctrl_chan.scid ||
        pkt->l2cap_hdr.cid == device->intr_chan.scid) {
        bt_hid_hdlr(device, pkt, pkt_len);
    }
}

/*
 * @brief: BT controller callback function, used to notify the upper layer that
 *         controller is ready to receive command
 */
static void bt_host_tx_pkt_ready(void) {
    atomic_set_bit(&bt_flags, BT_CTRL_READY);
}

/*
 * @brief: BT controller callback function, to transfer data packet to upper
 *         controller is ready to receive command
 */
static int bt_host_rx_pkt(uint8_t *data, uint16_t len) {
    struct bt_hci_pkt *bt_hci_pkt = (struct bt_hci_pkt *)data;
    bt_mon_tx((bt_hci_pkt->h4_hdr.type == BT_HCI_H4_TYPE_EVT) ? BT_MON_EVT : BT_MON_ACL_RX,
        data + 1, len - 1);

#ifdef CONFIG_BLUERETRO_BT_TIMING_TESTS
    if (atomic_test_bit(&bt_flags, BT_HOST_DBG_MODE)) {
        bt_dbg(data, len);
    }
    else {
#endif
    switch(bt_hci_pkt->h4_hdr.type) {
        case BT_HCI_H4_TYPE_ACL:
            bt_host_acl_hdlr(bt_hci_pkt, len);
            break;
        case BT_HCI_H4_TYPE_EVT:
            bt_hci_evt_hdlr(bt_hci_pkt);
            break;
        default:
            printf("# %s unsupported packet type: 0x%02X\n", __FUNCTION__, bt_hci_pkt->h4_hdr.type);
            break;
    }
#ifdef CONFIG_BLUERETRO_BT_TIMING_TESTS
    }
#endif

    return 0;
}

uint32_t bt_host_get_flag_dev_cnt(uint32_t flag) {
    uint32_t cnt = 0;
    for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
        if (atomic_test_bit(&bt_dev[i].flags, flag)) {
            cnt++;
        }
    }
    return cnt;
}

void bt_host_disconnect_all(void) {
    printf("# %s BOOT SW pressed, DISCONN all devices!\n", __FUNCTION__);
    for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
        if (atomic_test_bit(&bt_dev[i].flags, BT_DEV_DEVICE_FOUND)) {
            bt_hci_disconnect(&bt_dev[i]);
        }
    }
}

int32_t bt_host_get_new_dev(struct bt_dev **device) {
    for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
        if (!atomic_test_bit(&bt_dev[i].flags, BT_DEV_DEVICE_FOUND)) {
            *device = &bt_dev[i];
            return i;
        }
    }
    return -1;
}

int32_t bt_host_get_active_dev(struct bt_dev **device) {
    for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
        if (atomic_test_bit(&bt_dev[i].flags, BT_DEV_DEVICE_FOUND)) {
            *device = &bt_dev[i];
            return i;
        }
    }
    return -1;
}

int32_t bt_host_get_hid_init_dev(struct bt_dev **device) {
    for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
        if (atomic_test_bit(&bt_dev[i].flags, BT_DEV_HID_INIT_DONE)) {
            *device = &bt_dev[i];
            return i;
        }
    }
    return -1;
}

int32_t bt_host_get_dev_from_bdaddr(uint8_t *bdaddr, struct bt_dev **device) {
    for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
        if (atomic_test_bit(&bt_dev[i].flags, BT_DEV_DEVICE_FOUND) && memcmp((void *)bdaddr, bt_dev[i].remote_bdaddr, 6) == 0) {
            *device = &bt_dev[i];
            return i;
        }
    }
    return -1;
}

int32_t bt_host_get_dev_from_handle(uint16_t handle, struct bt_dev **device) {
    for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
        if (atomic_test_bit(&bt_dev[i].flags, BT_DEV_DEVICE_FOUND) && bt_acl_handle(handle) == bt_dev[i].acl_handle) {
            *device = &bt_dev[i];
            return i;
        }
    }
    return -1;
}

int32_t bt_host_get_dev_from_id(uint8_t id, struct bt_dev **device) {
    if (id < BT_MAX_DEV) {
        *device = &bt_dev[id];
        return id;
    }
    return -1;
}

int32_t bt_host_get_dev_from_out_idx(uint8_t out_idx, struct bt_dev **device) {
    for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
        if (bt_dev[i].ids.out_idx == out_idx) {
            *device = &bt_dev[i];
            return i;
        }
    }
    return -1;
}

int32_t bt_host_get_active_dev_from_out_idx(uint8_t out_idx, struct bt_dev **device) {
    for (uint32_t i = 0; i < BT_MAX_DEV; i++) {
        if (atomic_test_bit(&bt_dev[i].flags, BT_DEV_HID_INIT_DONE) && bt_dev[i].ids.out_idx == out_idx) {
            *device = &bt_dev[i];
            return i;
        }
    }
    return -1;
}

int32_t bt_host_get_dev_conf(struct bt_dev **device) {
    *device = &bt_dev_conf;
    return 0;
}

void bt_host_reset_dev(struct bt_dev *device) {
    int32_t dev_id = 0;

    for (; dev_id < BT_MAX_DEV; dev_id++) {
        if (device == &bt_dev[dev_id]) {
            goto reset_dev;
        }
    }
    return;

reset_dev:
    adapter_init_buffer(dev_id);
    memset(bt_adapter.data[dev_id].raw_src_mappings, 0, sizeof(*bt_adapter.data[0].raw_src_mappings) * REPORT_MAX);
    memset(bt_adapter.data[dev_id].reports, 0, sizeof(bt_adapter.data[0].reports));
    memset(&bt_adapter.data[dev_id].base, 0, sizeof(bt_adapter.data[0].base));
    memset(device, 0, sizeof(*device));

    device->ids.id = dev_id;
    device->ids.out_idx = dev_id;
    device->ids.type = BT_NONE;
    device->ids.subtype = BT_SUBTYPE_DEFAULT;
    bt_adapter.data[dev_id].base.pids = &device->ids;
}

void bt_host_q_wait_pkt(uint32_t ms) {
    uint8_t packet[2] = {0xFF, ms};

    bt_host_txq_add(packet, sizeof(packet));
}

int32_t bt_host_init(void) {
    int32_t ret;

#ifdef CONFIG_BLUERETRO_BT_TIMING_TESTS
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = 1ULL << 26;
    gpio_config(&io_conf);
    gpio_set_level(26, 1);
#endif

    bt_host_load_bdaddr_from_nvs();

    bt_mon_init();

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        printf("# Bluetooth controller initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM)) != ESP_OK) {
        printf("# Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_vhci_host_register_callback(&vhci_host_cb);

    bt_host_tx_pkt_ready();

    txq_hdl = xRingbufferCreate(256*8, RINGBUF_TYPE_NOSPLIT);
    if (txq_hdl == NULL) {
        printf("# Failed to create txq ring buffer\n");
        return -1;
    }

    bt_host_load_keys_from_file(&bt_host_link_keys);
    bt_host_load_le_keys_from_file(&bt_host_le_link_keys);

    xTaskCreatePinnedToCore(&bt_host_task, "bt_host_task", 3072, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(&bt_fb_task, "bt_fb_task", 3072, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(&bt_tx_task, "bt_tx_task", 2048, NULL, 11, NULL, 0);

    if (bt_hci_init()) {
        printf("# HCI init fail.\n");
        return -1;
    }

    return ret;
}

int32_t bt_host_txq_add(uint8_t *packet, uint32_t packet_len) {
#ifdef CONFIG_BLUERETRO_QEMU
    return 0;
#else
    UBaseType_t ret = xRingbufferSend(txq_hdl, (void *)packet, packet_len, portMAX_DELAY);
    if (ret != pdTRUE) {
        printf("# %s txq full!\n", __FUNCTION__);
    }
    return (ret == pdTRUE ? 0 : -1);
#endif
}

int32_t bt_host_load_link_key(struct bt_hci_cp_link_key_reply *link_key_reply) {
    int32_t ret = -1;
    for (uint32_t i = 0; i < ARRAY_SIZE(bt_host_link_keys.link_keys); i++) {
        if (memcmp((void *)&link_key_reply->bdaddr, (void *)&bt_host_link_keys.link_keys[i].bdaddr, sizeof(link_key_reply->bdaddr)) == 0) {
            memcpy((void *)link_key_reply->link_key, &bt_host_link_keys.link_keys[i].link_key, sizeof(link_key_reply->link_key));
            ret = 0;
        }
    }
    return ret;
}

int32_t bt_host_store_link_key(struct bt_hci_evt_link_key_notify *link_key_notify) {
    int32_t ret = -1;
    uint32_t index = bt_host_link_keys.index;
    for (uint32_t i = 0; i < ARRAY_SIZE(bt_host_link_keys.link_keys); i++) {
        if (memcmp((void *)&link_key_notify->bdaddr, (void *)&bt_host_link_keys.link_keys[i].bdaddr, sizeof(link_key_notify->bdaddr)) == 0) {
            index = i;
        }
    }
    memcpy((void *)&bt_host_link_keys.link_keys[index], (void *)link_key_notify, sizeof(bt_host_link_keys.link_keys[0]));
    if (index == bt_host_link_keys.index) {
        bt_host_link_keys.index++;
        bt_host_link_keys.index &= 0xF;
    }
    ret = bt_host_store_keys_on_file(&bt_host_link_keys);
    return ret;
}

void bt_host_clear_le_ltk(bt_addr_le_t *le_bdaddr) {
    for (uint32_t i = 0; i < ARRAY_SIZE(bt_host_le_link_keys.keys); i++) {
        if (memcmp((void *)le_bdaddr, (void *)&bt_host_le_link_keys.keys[i].le_bdaddr, sizeof(*le_bdaddr)) == 0) {
            memset(&bt_host_le_link_keys.keys[i].le_bdaddr, 0, sizeof(bt_host_le_link_keys.keys[0].le_bdaddr));
            memset(&bt_host_le_link_keys.keys[i].ltk, 0, sizeof(bt_host_le_link_keys.keys[i].ltk));
            memset(&bt_host_le_link_keys.keys[i].ident, 0, sizeof(bt_host_le_link_keys.keys[i].ident));
        }
    }
    bt_host_store_le_keys_on_file(&bt_host_le_link_keys);
}

int32_t bt_host_load_le_ltk(bt_addr_le_t *le_bdaddr, struct bt_smp_encrypt_info *encrypt_info, struct bt_smp_master_ident *master_ident) {
    int32_t ret = -1;
    for (uint32_t i = 0; i < ARRAY_SIZE(bt_host_le_link_keys.keys); i++) {
        if (memcmp((void *)le_bdaddr, (void *)&bt_host_le_link_keys.keys[i].le_bdaddr, sizeof(*le_bdaddr)) == 0) {
            if (encrypt_info) {
                memcpy((void *)encrypt_info, &bt_host_le_link_keys.keys[i].ltk, sizeof(*encrypt_info));
            }
            if (master_ident) {
                memcpy((void *)master_ident, &bt_host_le_link_keys.keys[i].ident, sizeof(*master_ident));
            }
            ret = 0;
        }
    }
    return ret;
}

int32_t bt_host_store_le_ltk(bt_addr_le_t *le_bdaddr, struct bt_smp_encrypt_info *encrypt_info) {
    int32_t ret = -1;
    uint32_t index = bt_host_le_link_keys.index;
    for (uint32_t i = 0; i < ARRAY_SIZE(bt_host_le_link_keys.keys); i++) {
        if (memcmp((void *)le_bdaddr, (void *)&bt_host_le_link_keys.keys[i].le_bdaddr, sizeof(*le_bdaddr)) == 0) {
            index = i;
        }
    }
    memcpy((void *)&bt_host_le_link_keys.keys[index].ltk, (void *)encrypt_info, sizeof(bt_host_le_link_keys.keys[0].ltk));
    memcpy((void *)&bt_host_le_link_keys.keys[index].le_bdaddr, (void *)le_bdaddr, sizeof(bt_host_le_link_keys.keys[0].le_bdaddr));
    if (index == bt_host_le_link_keys.index) {
        bt_host_le_link_keys.index++;
        bt_host_le_link_keys.index &= 0xF;
    }
    ret = bt_host_store_le_keys_on_file(&bt_host_le_link_keys);
    return ret;
}

int32_t bt_host_store_le_ident(bt_addr_le_t *le_bdaddr, struct bt_smp_master_ident *master_ident) {
    int32_t ret = -1;
    uint32_t index = bt_host_le_link_keys.index;
    for (uint32_t i = 0; i < ARRAY_SIZE(bt_host_le_link_keys.keys); i++) {
        if (memcmp((void *)le_bdaddr, (void *)&bt_host_le_link_keys.keys[i].le_bdaddr, sizeof(*le_bdaddr)) == 0) {
            index = i;
        }
    }
    memcpy((void *)&bt_host_le_link_keys.keys[index].ident, (void *)master_ident, sizeof(bt_host_le_link_keys.keys[0].ident));
    memcpy((void *)&bt_host_le_link_keys.keys[index].le_bdaddr, (void *)le_bdaddr, sizeof(bt_host_le_link_keys.keys[0].le_bdaddr));
    if (index == bt_host_le_link_keys.index) {
        bt_host_le_link_keys.index++;
        bt_host_le_link_keys.index &= 0xF;
    }
    ret = bt_host_store_le_keys_on_file(&bt_host_le_link_keys);
    return ret;
}

int32_t bt_host_get_next_accept_le_bdaddr(bt_addr_le_t *le_bdaddr) {
    static uint32_t index = 0;
    while (index < ARRAY_SIZE(bt_host_le_link_keys.keys)) {
        if (bt_addr_le_cmp(&bt_host_le_link_keys.keys[index].le_bdaddr, BT_ADDR_LE_ANY) != 0) {
            bt_addr_le_copy(le_bdaddr, &bt_host_le_link_keys.keys[index].le_bdaddr);
            index++;
            return 0;
        }
        index++;
    }
    return -1;
}

void bt_host_bridge(struct bt_dev *device, uint8_t report_id, uint8_t *data, uint32_t len) {
    struct bt_data *bt_data = &bt_adapter.data[device->ids.id];
    uint32_t report_type = PAD;

#ifdef CONFIG_BLUERETRO_BT_TIMING_TESTS
    atomic_set_bit(&bt_flags, BT_HOST_DBG_MODE);
    bt_dbg_init(device->ids.type);
#else
#ifdef CONFIG_BLUERETRO_RAW_REPORT_DUMP
    printf("# ");
    for (uint32_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
#else
    if (device->ids.type == BT_HID_GENERIC) {
        struct hid_report *report = hid_parser_get_report(device->ids.id, report_id);
        if (report == NULL || report->type == REPORT_NONE) {
            return;
        }
        bt_data->base.report_type = report_type = report->type;
        len = report->len;
        if (report_id != bt_data->base.report_id) {
            atomic_clear_bit(&bt_data->base.flags[report_type], BT_INIT);
        }
    }
    if (atomic_test_bit(&bt_data->base.flags[report_type], BT_INIT) || bt_data->base.report_cnt > 1) {
        bt_data->base.report_id = report_id;
        bt_data->base.input = data;
        bt_data->base.input_len = len;
        adapter_bridge(bt_data);
    }
    bt_data->base.report_cnt++;
#endif
#endif
}
