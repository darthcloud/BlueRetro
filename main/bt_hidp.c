#include "bt_host.h"

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
