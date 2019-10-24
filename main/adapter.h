#ifndef _ADAPTER_H_
#define _ADAPTER_H_

#include "zephyr/atomic.h"

#ifndef __packed
#define __packed    __attribute__((__packed__))
#endif

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

/* BT flags */
enum {
    BT_FEEDBACK,
};

enum {
    NONE_TYPE = -1,
    U8_TYPE,
    U16_TYPE,
    U32_TYPE,
    S8_TYPE,
    S16_TYPE,
    S32_TYPE,
};

enum {
    BTNS0,
    BTNS1,
    LX_AXIS,
    LY_AXIS,
    RX_AXIS,
    RY_AXIS,
    LT_AXIS,
    RT_AXIS,
    MAX_INPUT,
};

struct ctrl_data {
    uint8_t type;
    union {
        uint8_t *u8;
        uint16_t *u16;
        uint32_t *u32;
        int8_t *s8;
        int16_t *s16;
        int32_t *s32;
    } pval;
};

struct ctrl_desc {
    struct ctrl_data data[MAX_INPUT];
};

#define CTRL_DATA(input) \
    ({ __typeof__ (input) _input = (input); \
        _input.type == U8_TYPE ? *_input.pval.u8 : \
        _input.type == U16_TYPE ? *_input.pval.u16 : \
        _input.type == U32_TYPE ? *_input.pval.u32 : \
        _input.type == S8_TYPE ? *_input.pval.s8 : \
        _input.type == S16_TYPE ? *_input.pval.s16 :\
        *_input.pval.s32; \
    })

struct bt_data {
    /* Bi-directional */
    atomic_t flags;
    /* from adapter */
    uint8_t output[64];
    /* from wireless */
    int32_t dev_id;
    int32_t dev_type;
    uint8_t input[64];
    uint32_t hid_desc_len;
    uint8_t hid_desc[1024];
    struct ctrl_desc ctrl_desc;
} __packed;

struct wired_data {
    /* Bi-directional */
    atomic_t flags;
    /* from wired driver */
    uint32_t frame_cnt;
    uint8_t input[64];
    /* from adapter */
    int32_t dev_mode;
    uint8_t output[64];
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

extern struct bt_adapter bt_adapter;
extern struct wired_adapter wired_adapter;

void adapter_bridge(struct bt_data *bt_data);

#endif /* _ADAPTER_H_ */
