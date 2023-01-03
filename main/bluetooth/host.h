/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HOST_H_
#define _BT_HOST_H_

#include "zephyr/atomic.h"
#include "zephyr/hci.h"
#include "zephyr/l2cap_internal.h"
#include "zephyr/sdp_internal.h"
#include "zephyr/smp.h"
#include "zephyr/att_internal.h"
#include "adapter/adapter.h"
#include "hidp/hidp.h"

#define BT_MAX_RETRY 3
#define BT_SDP_DATA_SIZE 2048

enum {
    /* BT device connection flags */
    BT_DEV_DEVICE_FOUND,
    BT_DEV_PAGE,
    BT_DEV_ENCRYPTION,
    BT_DEV_SDP_TX_PENDING,
    BT_DEV_HID_CTRL_PENDING,
    BT_DEV_HID_INTR_PENDING,
    BT_DEV_HID_INTR_READY,
    BT_DEV_HID_INIT_DONE,
    BT_DEV_SDP_DATA,
    BT_DEV_ROLE_SW_FAIL,
    BT_DEV_IS_BLE,
    BT_DEV_FB_DELAY,
    BT_DEV_REPORT_MON,
};

struct bt_name_type {
    char name[249];
    int32_t type;
    uint32_t subtype;
    atomic_t hid_flags;
};

struct l2cap_chan {
    uint16_t scid;
    uint16_t dcid;
    uint16_t mtu;
};

struct bt_dev {
    int32_t flags;
    struct bt_ids ids;
    uint32_t pkt_retry;
    union {
        struct {
            uint8_t reserve;
            uint8_t remote_bdaddr[6];
        };
        bt_addr_le_t le_remote_bdaddr;
    };
    uint16_t acl_handle;
    uint32_t hid_state;
    void *timer_hdl;
    uint8_t report_stall_cnt;
    uint8_t tid;
    const struct bt_name_type *name;
    union {
        struct {
            uint32_t sdp_state;
            struct l2cap_chan sdp_rx_chan;
            struct l2cap_chan sdp_tx_chan;
            struct l2cap_chan ctrl_chan;
            struct l2cap_chan intr_chan;
        };
        struct {
            uint8_t rand[BT_SMP_MAX_ENC_KEY_SIZE];
            uint8_t rrand[BT_SMP_MAX_ENC_KEY_SIZE];
            uint8_t ltk[BT_SMP_MAX_ENC_KEY_SIZE];
            uint8_t preq[7];
            uint8_t pres[7];
            uint8_t rdist;
            uint8_t ldist;
            uint16_t mtu;
        };
    };
};

struct bt_hci_pkt {
    struct bt_hci_h4_hdr h4_hdr;
    union {
        struct {
            struct bt_hci_cmd_hdr cmd_hdr;
            uint8_t cp[1020];
        };
        struct {
            struct bt_hci_acl_hdr acl_hdr;
            struct bt_l2cap_hdr l2cap_hdr;
            union {
                struct {
                    struct bt_l2cap_sig_hdr sig_hdr;
                    uint8_t sig_data[1011];
                };
                struct {
                    struct bt_smp_hdr smp_hdr;
                    uint8_t smp_data[1014];
                };
                struct {
                    struct bt_att_hdr att_hdr;
                    uint8_t att_data[1014];
                };
                struct {
                    struct bt_sdp_hdr sdp_hdr;
                    uint8_t sdp_data[1010];
                };
                struct {
                    struct bt_hidp_hdr hidp_hdr;
                    uint8_t hidp_data[1013];
                };
            };
        };
        struct {
            struct bt_hci_evt_hdr evt_hdr;
            uint8_t evt_data[1021];
        };
    };
} __packed;

extern struct bt_hci_pkt bt_hci_pkt_tmp;

void bt_host_disconnect_all(void);
int32_t bt_host_get_new_dev(struct bt_dev **device);
int32_t bt_host_get_active_dev(struct bt_dev **device);
int32_t bt_host_get_hid_init_dev(struct bt_dev **device);
int32_t bt_host_get_dev_from_bdaddr(uint8_t *bdaddr, struct bt_dev **device);
int32_t bt_host_get_dev_from_handle(uint16_t handle, struct bt_dev **device);
int32_t bt_host_get_dev_from_id(uint8_t id, struct bt_dev **device);
int32_t bt_host_get_dev_from_out_idx(uint8_t out_idx, struct bt_dev **device);
int32_t bt_host_get_dev_conf(struct bt_dev **device);
void bt_host_reset_dev(struct bt_dev *device);
void bt_host_q_wait_pkt(uint32_t ms);
int32_t bt_host_init(void);
int32_t bt_host_txq_add(uint8_t *packet, uint32_t packet_len);
int32_t bt_host_load_link_key(struct bt_hci_cp_link_key_reply *link_key_reply);
int32_t bt_host_store_link_key(struct bt_hci_evt_link_key_notify *link_key_notify);
int32_t bt_host_load_le_ltk(bt_addr_le_t *le_bdaddr, struct bt_smp_encrypt_info *encrypt_info, struct bt_smp_master_ident *master_ident);
int32_t bt_host_store_le_ltk(bt_addr_le_t *le_bdaddr, struct bt_smp_encrypt_info *encrypt_info);
int32_t bt_host_store_le_ident(bt_addr_le_t *le_bdaddr, struct bt_smp_master_ident *master_ident);
int32_t bt_host_get_next_accept_le_bdaddr(bt_addr_le_t *le_bdaddr);
void bt_host_bridge(struct bt_dev *device, uint8_t report_id, uint8_t *data, uint32_t len);

#endif /* _BT_HOST_H_ */

