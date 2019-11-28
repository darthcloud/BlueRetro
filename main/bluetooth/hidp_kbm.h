#ifndef _BT_HIDP_KBM_H_
#define _BT_HIDP_KBM_H_

#include "hidp.h"

#define BT_HIDP_KB_STATUS 0x01
struct bt_hidp_kb_status {
    uint8_t data[8];
} __packed;

#define BT_HIDP_M_STATUS 0x02
struct bt_hidp_m_status {
    uint8_t data[5];
} __packed;

void bt_hid_kbm_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt);

#endif /* _BT_HIDP_KBM_H_ */
