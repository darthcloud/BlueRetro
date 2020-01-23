#ifndef _BT_SDP_H_
#define _BT_SDP_H_

void bt_sdp_cmd_svc_search_attr_req(struct bt_dev *device, uint8_t *cont_data, uint32_t cont_len);
void bt_sdp_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt);

#endif /* _BT_SDP_H_ */
