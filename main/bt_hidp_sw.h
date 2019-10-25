#ifndef _BT_HIDP_SW_H_
#define _BT_HIDP_SW_H_

#include "bt_hidp.h"

#define BT_HIDP_SW_SET_CONF 0x01
#define BT_HIDP_SW_SUBCMD_SET_REP_MODE 0x03
#define BT_HIDP_SW_SUBCMD_SET_LED 0x30
#define BT_HIDP_SW_SUBCMD_EN_RUMBLE 0x48
struct bt_hidp_sw_conf {
    uint8_t tid;
    uint8_t rumble[8];
    uint8_t subcmd;
    uint8_t subcmd_data[38];
} __packed;

#define BT_HIDP_SW_SUBCMD_ACK 0x21
struct bt_hidp_sw_subcmd_ack {
    uint8_t tbd[13];
    uint8_t subcmd;
    uint8_t tbd2[34];
} __packed;

#define BT_HIDP_SW_STATUS 0x3F
struct bt_hidp_sw_status {
    uint8_t data[11];
} __packed;

void bt_hid_cmd_sw_set_conf(struct bt_dev *device, void *report);
void bt_hid_sw_init(struct bt_dev *device);
void bt_hid_sw_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt);

#endif /* _BT_HIDP_SW_H_ */
