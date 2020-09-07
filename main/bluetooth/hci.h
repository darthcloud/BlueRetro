#ifndef _BT_HCI_H_
#define _BT_HCI_H_

void bt_hci_init(void);
void bt_hci_disconnect(struct bt_dev *device);
void bt_hci_evt_hdlr(struct bt_hci_pkt *bt_hci_evt_pkt);

#endif /* _BT_HCI_H_ */
