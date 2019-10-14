#ifndef _BT_HIDP_H_
#define _BT_HIDP_H_

typedef void (*bt_hid_cmd_func_t)(void *bt_dev, void *report);

struct bt_hidp_cmd {
    bt_hid_cmd_func_t cmd;
    uint32_t sync;
    void *report;
};


#define BT_HIDP_DATA_IN        0xa1
#define BT_HIDP_DATA_OUT       0xa2
struct bt_hidp_hdr {
    uint8_t hdr;
    uint8_t protocol;
} __packed;

int8_t bt_hid_minor_class_to_type(uint8_t minor);
void bt_hid_cmd(uint16_t handle, uint16_t cid, uint8_t protocol, uint16_t len);

#endif /* _BT_HIDP_H_ */
