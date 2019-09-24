#ifndef _ADAPTER_H_
#define _ADAPTER_H_

#include "zephyr/atomic.h"

#define BT_MAX_DEV   7

/* BT device ID */
#define BT_NONE      -1
#define WII_CORE     0x00
#define WII_NUNCHUCK 0x01
#define WII_CLASSIC  0x02
#define WIIU_PRO     0x03
#define SWITCH_PRO   0x04
#define PS3_DS3      0x05
#define PS4_DS4      0x06
#define XBONE_S      0x07
#define HID_PAD      0x08
#define HID_KB       0x09
#define HID_MOUSE    0x0A

/* Wired system ID */
#define WIRED_NONE   -1
#define NES          0x00
#define SNES         0x01
#define N64          0x02
#define GC           0x03
#define WII_EXT      0x04
#define SMS          0x05
#define GENESIS      0x06
#define SATURN       0x07
#define DC           0x08
#define PCE          0x09
#define PSX          0x0A
#define PS2          0x0B

struct bt_data {
    /* Bi-directional */
    atomic_t flags;
    /* from adapter */
    uint8_t output[4];
    /* from wireless */
    uint8_t input[24];
} __packed;

struct wired_data {
    /* Bi-directional */
    atomic_t flags;
    /* from wired driver */
    uint32_t frame_cnt;
    uint8_t input[4];
    /* from adapter */
    uint32_t acc_mode;
    uint8_t output[24];
} __packed;

struct wired_adapter {
    /* from wired driver */
    uint32_t system_id;
    /* from adapter */
    uint32_t dev_mode;
    /* Bi-directional */
    struct wired_data data[BT_MAX_DEV];
};

struct bt_adapter {
    struct bt_data data[BT_MAX_DEV];
};

#endif /* _ADAPTER_H_ */
