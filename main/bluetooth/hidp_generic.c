#include "host.h"
#include "hidp_generic.h"

void bt_hid_generic_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
#if 0
                case BT_HIDP_KB_STATUS:
                {
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, sizeof(struct bt_hidp_kb_status));
                    break;
                }
                case BT_HIDP_M_STATUS:
                {
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, sizeof(struct bt_hidp_m_status));
                    break;
                }
#endif
            }
            break;
    }
}
