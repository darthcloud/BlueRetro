#ifndef _BT_ATT_H_
#define _BT_ATT_H_

void bt_att_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);

#endif /* _BT_ATT_H_ */
