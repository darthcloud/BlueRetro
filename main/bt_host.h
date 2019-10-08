#ifndef _BT_HOST_H_
#define _BT_HOST_H_

int32_t bt_host_init(void);
int32_t bt_host_txq_add(uint8_t *packet, uint32_t packet_len);

#endif /* _BT_HOST_H_ */

