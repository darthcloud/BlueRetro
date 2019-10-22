#include "bt_host.h"
#include "bt_hidp_wii.h"

typedef void (*bt_hid_hdlr_t)(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt);

const uint8_t bt_hid_led_dev_id_map[] = {
    0x1, 0x2, 0x4, 0x8, 0x3, 0x6, 0xC
};

static const struct bt_hidp_cmd (*bt_hid_conf_list[BT_MAX])[BT_MAX_HID_CONF_CMD] = {
    NULL, /* HID_PAD */
    NULL, /* HID_KB */
    NULL, /* HID_MOUSE */
    NULL, /* PS3_DS3 */
    &bt_hipd_wii_conf, /* WII_CORE */
    &bt_hipd_wii_conf, /* WII_NUNCHUCK */
    &bt_hipd_wii_conf, /* WII_CLASSIC */
    &bt_hipd_wii_conf, /* WIIU_PRO */
    NULL, /* PS4_DS4 */
    NULL, /* XB1_S */
    NULL, /* SWITCH_PRO */
};

static const bt_hid_hdlr_t bt_hid_hdlr_list[BT_MAX] = {
    NULL, /* HID_PAD */
    NULL, /* HID_KB */
    NULL, /* HID_MOUSE */
    NULL, /* PS3_DS3 */
    bt_hid_wii_hdlr, /* WII_CORE */
    bt_hid_wii_hdlr, /* WII_NUNCHUCK */
    bt_hid_wii_hdlr, /* WII_CLASSIC */
    bt_hid_wii_hdlr, /* WIIU_PRO */
    NULL, /* PS4_DS4 */
    NULL, /* XB1_S */
    NULL, /* SWITCH_PRO */
};

static const bt_hid_cmd_func_t bt_hid_feedback_list[BT_MAX] = {
    NULL, /* HID_PAD */
    NULL, /* HID_KB */
    NULL, /* HID_MOUSE */
    NULL, /* PS3_DS3 */
    bt_hid_cmd_wii_set_feedback, /* WII_CORE */
    bt_hid_cmd_wii_set_feedback, /* WII_NUNCHUCK */
    bt_hid_cmd_wii_set_feedback, /* WII_CLASSIC */
    bt_hid_cmd_wii_set_feedback, /* WIIU_PRO */
    NULL, /* PS4_DS4 */
    NULL, /* XB1_S */
    NULL, /* SWITCH_PRO */
};

void bt_hid_config(struct bt_dev *device) {
    if (device->type > BT_NONE && bt_hid_conf_list[device->type] && (*bt_hid_conf_list[device->type])) {
        for (uint32_t i = 0; i < BT_MAX_HID_CONF_CMD && (*bt_hid_conf_list[device->type])[i].cmd; i++) {
            (*bt_hid_conf_list[device->type])[i].cmd(device, (*bt_hid_conf_list[device->type])[i].report);
        }
    }
}

void bt_hid_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    if (device->type > BT_NONE && bt_hid_hdlr_list[device->type]) {
        bt_hid_hdlr_list[device->type](device, bt_hci_acl_pkt);
    }
}

void bt_hid_feedback(struct bt_dev *device, void *report) {
    if (device->type > BT_NONE && bt_hid_feedback_list[device->type]) {
        bt_hid_feedback_list[device->type](device, report);
    }
}

int8_t bt_hid_minor_class_to_type(uint8_t minor) {
    int8_t type = HID_PAD;
    if (minor & 0x80) {
        type = HID_MOUSE;
    }
    else if (minor & 0x40) {
        type = HID_KB;
    }
    return type;
}

void bt_hid_cmd(uint16_t handle, uint16_t cid, uint8_t protocol, uint16_t len) {
    uint16_t packet_len = (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
        + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_hidp_hdr) + len);

    bt_hci_pkt_tmp.h4_hdr.type = BT_HCI_H4_TYPE_ACL;

    bt_hci_pkt_tmp.acl_hdr.handle = bt_acl_handle_pack(handle, 0x2);
    bt_hci_pkt_tmp.acl_hdr.len = packet_len - BT_HCI_H4_HDR_SIZE - BT_HCI_ACL_HDR_SIZE;

    bt_hci_pkt_tmp.l2cap_hdr.len = bt_hci_pkt_tmp.acl_hdr.len - sizeof(bt_hci_pkt_tmp.l2cap_hdr);
    bt_hci_pkt_tmp.l2cap_hdr.cid = cid;

    bt_hci_pkt_tmp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_hci_pkt_tmp.hidp_hdr.protocol = protocol;

    bt_host_txq_add((uint8_t *)&bt_hci_pkt_tmp, packet_len);
}
