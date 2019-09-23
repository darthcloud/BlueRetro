#ifndef _ADAPTER_H_
#define _ADAPTER_H_

#include "zephyr/atomic.h"

/* BT device ID */
#define WII_CORE     0x01
#define WII_NUNCHUCK 0x02
#define WII_CLASSIC  0x03
#define WIIU_PRO     0x04
#define SWITCH_PRO   0x05
#define PS3_DS3      0x06
#define PS4_DS4      0x07
#define XONE_S       0x08

struct adapter {
    /* Bi-directional */
    atomic_t flags;
    /* from wired driver */
    uint32_t frame_cnt;
    uint8_t native_fb[4];
    /* from adapter */
    uint8_t native_map[24];
} __packed;

struct shared_mem {
    /* from wired driver */
    uint32_t system_id;
    /* from adapter */
    uint32_t mode;
    /* Bi-directional */
    struct adapter ctrl[7];
};

#endif /* _ADAPTER_H_ */
