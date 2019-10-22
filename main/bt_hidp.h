#ifndef _BT_HIDP_H_
#define _BT_HIDP_H_

#define BT_MAX_HID_CONF_CMD 8

#define BT_HIDP_DATA_IN        0xa1
#define BT_HIDP_DATA_OUT       0xa2

struct bt_dev;
struct bt_hci_pkt;

typedef void (*bt_hid_cmd_func_t)(struct bt_dev *device, void *report);

struct bt_hidp_cmd {
    bt_hid_cmd_func_t cmd;
    void *report;
};

struct bt_hidp_hdr {
    uint8_t hdr;
    uint8_t protocol;
} __packed;

extern const uint8_t bt_hid_led_dev_id_map[];

void bt_hid_config(struct bt_dev *device);
void bt_hid_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt);
void bt_hid_feedback(struct bt_dev *device, void *report);
int8_t bt_hid_minor_class_to_type(uint8_t minor);
void bt_hid_cmd(uint16_t handle, uint16_t cid, uint8_t protocol, uint16_t len);

#endif /* _BT_HIDP_H_ */
