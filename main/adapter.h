#ifndef _ADAPTER_H_
#define _ADAPTER_H_

#include "zephyr/atomic.h"

#define BT_MAX_DEV   7 /* BT limitation */
#define WIRED_MAX_DEV 12 /* Saturn limit */

/* BT device ID */
enum {
    BT_NONE = -1,
    HID_PAD,
    HID_KB,
    HID_MOUSE,
    PS3_DS3,
    WII_CORE,
    WII_NUNCHUCK,
    WII_CLASSIC,
    WIIU_PRO,
    PS4_DS4,
    XB1_S,
    SWITCH_PRO,
    BT_MAX,
};

/* Wired system ID */
enum {
    WIRED_NONE = -1,
    NES,
    SMS,
    PCE,
    GENESIS,
    SNES,
    SATURN,
    PSX,
    N64,
    DC,
    PS2,
    GC,
    WII_EXT,
    WIRED_MAX,
};

struct bt_data {
    /* Bi-directional */
    atomic_t flags;
    /* from adapter */
    uint8_t output[4];
    /* from wireless */
    int32_t dev_id;
    int32_t dev_type;
    uint8_t input[24];
} __packed;

struct wired_data {
    /* Bi-directional */
    atomic_t flags;
    /* from wired driver */
    uint32_t frame_cnt;
    uint8_t input[4];
    /* from adapter */
    int32_t dev_mode;
    uint8_t output[24];
} __packed;

struct wired_adapter {
    /* from wired driver */
    int32_t system_id;
    /* from adapter */
    int32_t driver_mode;
    /* Bi-directional */
    struct wired_data data[WIRED_MAX_DEV];
};

struct bt_adapter {
    struct bt_data data[BT_MAX_DEV];
};

#endif /* _ADAPTER_H_ */
