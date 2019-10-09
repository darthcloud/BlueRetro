#ifndef _BT_HOST_H_
#define _BT_HOST_H_

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

int32_t bt_host_init(void);
int32_t bt_host_txq_add(uint8_t *packet, uint32_t packet_len);

#endif /* _BT_HOST_H_ */

