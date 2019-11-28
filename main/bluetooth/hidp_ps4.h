#ifndef _BT_HIDP_PS4_H_
#define _BT_HIDP_PS4_H_

#include "hidp.h"

#define BT_HIDP_PS4_STATUS 0x01
#define BT_HIDP_PS4_STATUS2 0x11
struct bt_hidp_ps4_status {
    uint8_t data[77];
} __packed;

#define BT_HIDP_PS4_SET_CONF 0x11
struct bt_hidp_ps4_set_conf {
    uint8_t conf0;
    uint8_t tbd0;
    uint8_t conf1;
    uint8_t tbd1[2];
    uint8_t r_rumble;
    uint8_t l_rumble;
    uint8_t leds[3];
    uint8_t led_on_delay;
    uint8_t led_off_delay;
    uint8_t tbd2[61];
    uint32_t crc;
} __packed;

void bt_hid_cmd_ps4_set_conf(struct bt_dev *device, void *report);
void bt_hid_ps4_init(struct bt_dev *device);
void bt_hid_ps4_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt);

#endif /* _BT_HIDP_PS4_H_ */
