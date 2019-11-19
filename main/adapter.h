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

enum {
    BTN_NONE = -1,
    PAD_LX_LEFT,
    PAD_LX_RIGHT,
    PAD_LY_DOWN,
    PAD_LY_UP,
    PAD_RX_LEFT,
    PAD_RX_RIGHT,
    PAD_RY_DOWN,
    PAD_RY_UP,
    PAD_LD_LEFT,
    PAD_LD_RIGHT,
    PAD_LD_DOWN,
    PAD_LD_UP,
    PAD_RD_LEFT,
    PAD_RD_RIGHT,
    PAD_RD_DOWN,
    PAD_RD_UP,
    PAD_RB_LEFT,
    PAD_RB_RIGHT,
    PAD_RB_DOWN,
    PAD_RB_UP,
    PAD_MM,
    PAD_MS,
    PAD_MT,
    PAD_MQ,
    PAD_LM,
    PAD_LS,
    PAD_LT,
    PAD_LJ,
    PAD_RM,
    PAD_RS,
    PAD_RT,
    PAD_RJ,
};

enum {
    MOUSE_NONE = -1,
    MOUSE_LY_DOWN = PAD_LY_DOWN,
    MOUSE_LY_UP,
    MOUSE_RX_LEFT = PAD_RX_LEFT,
    MOUSE_RX_RIGHT,
    MOUSE_RY_DOWN,
    MOUSE_RY_UP,
    MOUSE_LEFT = PAD_RB_LEFT,
    MOUSE_MIDDLE,
    MOUSE_RIGHT,
    MOUSE_4,
    MOUSE_5,
    MOUSE_6,
    MOUSE_7,
    MOUSE_8,
};

/* Stage the KB mapping to be a good default when used as a joystick */
enum {
    KB_NONE = -1,
    KB_A,
    KB_D,
    KB_S,
    KB_W,
    KB_J,
    KB_L,
    KB_K,
    KB_I,
    KB_LEFT,
    KB_RIGHT,
    KB_DOWN,
    KB_UP,
    KB_COMMA,
    KB_SLASH,
    KB_DOT,
    KB_SEMICOLON,
    KB_Q,
    KB_R,
    KB_E,
    KB_F,
    KB_ESC,
    KB_HASH,
    KB_LWIN,
    KB_APOSTROPHE,
    KB_Z,
    KB_TAB,
    KB_LEFTBRACE,
    KB_LCTRL,
    KB_X,
    KB_LSHIFT,
    KB_RIGHTBRACE,
    KB_SPACE,
    KB_B,
    KB_C,
    KB_G,
    KB_H,
    KB_M,
    KB_N,
    KB_O,
    KB_T,
    KB_U,
    KB_V,
    KB_Y,
    KB_1,
    KB_2,
    KB_3,
    KB_4,
    KB_5,
    KB_6,
    KB_7,
    KB_8,
    KB_9,
    KB_0,
    KB_ENTER,
    KB_BACKSPACE,
    KB_MINUS,
    KB_EQUAL,
    KB_BACKSLASH,
    KB_GRAVE,
    KB_CAPSLOCK,
    KB_F1,
    KB_F2,
    KB_F3,
    KB_F4,
    KB_F5,
    KB_F6,
    KB_F7,
    KB_F8,
    KB_F9,
    KB_F10,
    KB_F11,
    KB_F12,
    KB_PSCREEN,
    KB_SCROLL,
    KB_PAUSE,
    KB_INSERT,
    KB_HOME,
    KB_PAGEUP,
    KB_DEL,
    KB_END,
    KB_PAGE_DOWN,
    KB_NUMLOCK,
    KB_KP_DIV,
    KB_KP_MULTI,
    KB_KP_MINUS,
    KB_KP_PLUS,
    KB_KP_ENTER,
    KB_KP_1,
    KB_KP_2,
    KB_KP_3,
    KB_KP_4,
    KB_KP_5,
    KB_KP_6,
    KB_KP_7,
    KB_KP_8,
    KB_KP_9,
    KB_KP_0,
    KB_LALT,
    KB_RCTRL,
    KB_RSHIFT,
    KB_RALT,
    KB_RWIN,
    KB_MAX,
};

enum {
    AXIS_NONE = -1,
    AXIS_LX,
    AXIS_LY,
    AXIS_RX,
    AXIS_RY,
    TRIG_L,
    TRIG_R,
};

/* BT flags */
enum {
    BT_FEEDBACK,
};

/* Wired flags */
enum {
    WIRED_RUMBLE_ON,
    WIRED_SAVE_MEM,
};

#if 0
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
#endif

struct ctrl_meta {
    int32_t neutral;
    int32_t deadzone;
    int32_t abs_btn_thrs;
    int32_t abs_max;
    int32_t sign;
    int32_t polarity;
};

struct ctrl {
    int32_t value;
    const struct ctrl_meta *meta;
};

struct generic_ctrl {
    uint32_t *mask;
    uint32_t *desc;
    uint32_t map_mask[4];
    struct ctrl btns[4];
    struct ctrl axes[6];
};

#if 0
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
#endif

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
    //struct ctrl_desc ctrl_desc;
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

typedef void (*to_generic_t)(struct bt_data *bt_data, struct generic_ctrl *ctrl_data);
typedef void (*from_generic_t)(struct generic_ctrl *ctrl_data, struct wired_data *wired_data);
typedef void (*meta_init_t)(struct generic_ctrl *ctrl_data);

extern const uint32_t generic_btns_mask[32];
extern struct bt_adapter bt_adapter;
extern struct wired_adapter wired_adapter;

uint8_t btn_id_to_axis(uint8_t btn_id);
uint32_t axis_to_btn_mask(uint8_t axis);
int8_t btn_sign(uint32_t polarity, uint8_t btn_id);
void adapter_bridge(struct bt_data *bt_data);
void adapter_init(void);

#endif /* _ADAPTER_H_ */
