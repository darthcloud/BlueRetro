#ifndef _BT_HIDP_H_
#define _BT_HIDP_H_

//#include "bt_hidp_wii.h"

#define BT_HIDP_DATA_IN        0xa1
#define BT_HIDP_DATA_OUT       0xa2
struct bt_hidp_hdr {
    uint8_t hdr;
    uint8_t protocol;
} __packed;
#endif /* _BT_HIDP_H_ */
