#ifndef _BT_HOST_H_
#define _BT_HOST_H_

#include "zephyr/atomic.h"
#include "zephyr/hci.h"
#include "zephyr/l2cap_internal.h"
#include "zephyr/sdp_internal.h"
#include "adapter.h"
#include "bt_hidp.h"

#define BT_MAX_RETRY 3

enum {
    /* BT device connection flags */
    BT_DEV_DEVICE_FOUND,
    BT_DEV_PAGE,
    BT_DEV_ENCRYPTION,
    BT_DEV_SDP_TX_PENDING,
    BT_DEV_HID_CTRL_PENDING,
    BT_DEV_HID_INTR_PENDING,
    BT_DEV_ROLE_SW_FAIL,
};

struct l2cap_chan {
    uint16_t scid;
    uint16_t dcid;
};

struct bt_dev {
    int32_t id;
    int32_t flags;
    uint32_t pkt_retry;
    uint32_t report_cnt;
    uint8_t remote_bdaddr[6];
    int8_t type;
    uint16_t acl_handle;
    uint32_t sdp_state;
    uint32_t hid_state;
    struct l2cap_chan sdp_rx_chan;
    struct l2cap_chan sdp_tx_chan;
    struct l2cap_chan ctrl_chan;
    struct l2cap_chan intr_chan;
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

int32_t bt_host_get_new_dev(struct bt_dev **device);
int32_t bt_host_get_active_dev(struct bt_dev **device);
int32_t bt_host_get_dev_from_bdaddr(bt_addr_t *bdaddr, struct bt_dev **device);
int32_t bt_host_get_dev_from_handle(uint16_t handle, struct bt_dev **device);
void bt_host_reset_dev(struct bt_dev *device);
void bt_host_q_wait_pkt(uint32_t ms);
int32_t bt_host_init(void);
int32_t bt_host_txq_add(uint8_t *packet, uint32_t packet_len);
int32_t bt_host_load_link_key(struct bt_hci_cp_link_key_reply *link_key_reply);
int32_t bt_host_store_link_key(struct bt_hci_evt_link_key_notify *link_key_notify);
void bt_host_bridge(struct bt_dev *device, uint8_t *data, uint32_t len);

#endif /* _BT_HOST_H_ */

