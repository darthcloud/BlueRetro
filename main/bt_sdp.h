#ifndef _BT_SDP_H_
#define _BT_SDP_H_

void bt_sdp_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt);
void bt_sdp_cmd_svc_search_attr_req(void *bt_dev);

#endif /* _BT_SDP_H_ */
