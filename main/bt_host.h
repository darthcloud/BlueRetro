#ifndef _BT_HOST_H_
#define _BT_HOST_H_

#include "zephyr/hci.h"
#include "zephyr/l2cap_internal.h"
#include "bt_hidp.h"

struct l2cap_chan {
    uint16_t scid;
    uint16_t dcid;
    uint8_t  indent;
};

struct bt_dev {
    int32_t id;
    int32_t flags;
    uint32_t report_cnt;
    uint8_t remote_bdaddr[6];
    int8_t type;
    uint16_t acl_handle;
    uint8_t l2cap_ident;
    struct l2cap_chan sdp_chan;
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

int32_t bt_host_init(void);
int32_t bt_host_txq_add(uint8_t *packet, uint32_t packet_len);

#endif /* _BT_HOST_H_ */

