#include "host.h"
#include "hidp_generic.h"

void bt_hid_generic_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, sizeof(bt_hci_acl_pkt->hidp_data));
            break;
    }
}
