/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ADAPTER_H_
#define _ADAPTER_H_

#include <esp_attr.h>
#include "../zephyr/atomic.h"

#ifndef __packed
#define __packed    __attribute__((__packed__))
#endif

#define RESET   "\033[0m"
#define BOLD   "\033[1m\033[37m"

#define BT_MAX_DEV   7 /* BT limitation */
#define WIRED_MAX_DEV 12 /* Saturn limit */
#define ADAPTER_MAX_AXES 6
#define REPORT_MAX_USAGE 16

/* BT device ID */
enum {
    BT_NONE = -1,
    HID_GENERIC,
    PS3_DS3,
    WII_CORE,
    WII_NUNCHUCK,
    WII_CLASSIC,
    WIIU_PRO,
    PS4_DS4,
    XB1_S,
    XB1_ADAPTIVE,
    SW,
    BT_MAX,
};

/* Wired system ID */
enum {
    WIRED_NONE = -1,
    WIRED_AUTO,
    PARALLEL_1P,
    PARALLEL_2P,
    NES,
    PCE,
    GENESIS,
    SNES,
    CDI,
    CD32,
    REAL_3DO,
    JAGUAR,
    PSX,
    SATURN,
    PCFX,
    N64,
    DC,
    PS2,
    GC,
    WII_EXT,
    EXP_BOARD,
    WIRED_MAX,
};

/* Report type ID */
enum {
    REPORT_NONE = -1,
    KB,
    MOUSE,
    PAD,
    EXTRA,
    REPORT_MAX,
    //RUMBLE,
    //LEDS,
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

/* Stage the Mouse & KB mapping to be a good default when used as a joystick */
enum {
    KBM_NONE = -1,
    KB_A,
    KB_D,
    KB_S,
    KB_W,
    MOUSE_X_LEFT,
    MOUSE_X_RIGHT,
    MOUSE_Y_DOWN,
    MOUSE_Y_UP,
    KB_LEFT,
    KB_RIGHT,
    KB_DOWN,
    KB_UP,
    MOUSE_WX_LEFT,
    MOUSE_WX_RIGHT,
    MOUSE_WY_DOWN,
    MOUSE_WY_UP,
    KB_Q, MOUSE_4 = KB_Q,
    KB_R, MOUSE_5 = KB_R,
    KB_E, MOUSE_6 = KB_E,
    KB_F, MOUSE_7 = KB_F,
    KB_ESC,
    KB_ENTER,
    KB_LWIN,
    KB_HASH,
    MOUSE_RIGHT,
    KB_Z,
    KB_LCTRL,
    MOUSE_MIDDLE,
    MOUSE_LEFT,
    KB_X, MOUSE_8 = KB_X,
    KB_LSHIFT,
    KB_SPACE,
    KB_B,
    KB_C,
    KB_G,
    KB_H,
    KB_I,
    KB_J,
    KB_K,
    KB_L,
    KB_M,
    KB_N,
    KB_O,
    KB_P,
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
    KB_BACKSPACE,
    KB_TAB,
    KB_MINUS,
    KB_EQUAL,
    KB_LEFTBRACE,
    KB_RIGHTBRACE,
    KB_BACKSLASH,
    KB_SEMICOLON,
    KB_APOSTROPHE,
    KB_GRAVE,
    KB_COMMA,
    KB_DOT,
    KB_SLASH,
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
    KB_PAGEDOWN,
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
    KB_KP_DOT,
    KB_LALT,
    KB_RCTRL,
    KB_RSHIFT,
    KB_RALT,
    KB_RWIN,
    KBM_MAX,
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
    BT_INIT,
};

/* Wired flags */
enum {
    WIRED_NEW_DATA,
    WIRED_SAVE_MEM,
    WIRED_WAITING_FOR_RELEASE,
};

/* Dev mode */
enum {
    DEV_PAD = 0,
    DEV_PAD_ALT,
    DEV_KB,
    DEV_MOUSE,
};

/* Acc mode */
enum {
    ACC_NONE = 0,
    ACC_MEM,
    ACC_RUMBLE,
    ACC_BOTH,
};

/* Multitap mode */
enum {
    MT_NONE = 0,
    MT_SLOT_1,
    MT_SLOT_2,
    MT_DUAL,
    MT_ALT,
};

/* Scaling mode */
enum {
    LINEAR = 0,
    AGGRESSIVE,
    RELAXED,
    WIDE,
    S_CURVE,
    PASSTHROUGH,
};

/* Diagonal Scaling mode */
enum {
    DIAG_PASSTHROUGH = 0,
    CIRCLE_SQUARE,
    CIRCLE_HEX,
    SQUARE_CIRCLE,
    SQUARE_HEX,
};

struct ctrl_meta {
    int32_t neutral;
    int32_t deadzone;
    int32_t abs_btn_thrs;
    int32_t abs_max;
    int32_t sign;
    int32_t polarity;
    int32_t size_min;
    int32_t size_max;
};

struct ctrl {
    int32_t value;
    const struct ctrl_meta *meta;
};

struct generic_ctrl {
    const uint32_t *mask;
    const uint32_t *desc;
    uint32_t map_mask[4];
    struct ctrl btns[4];
    struct ctrl axes[ADAPTER_MAX_AXES];
};

struct generic_fb {
    uint32_t wired_id;
    uint32_t state;
    uint32_t cycles;
    uint32_t start;
};

struct raw_fb {
    uint8_t wired_id;
    uint8_t data[0];
};

struct hid_usage {
    uint8_t usage_page;
    uint16_t usage;
    uint8_t flags;
    uint32_t bit_offset;
    uint32_t bit_size;
    int32_t logical_min;
    int32_t logical_max;
};

struct hid_report {
    atomic_t flags;
    uint8_t id;
    uint32_t len;
    uint32_t usage_cnt;
    struct hid_usage usages[REPORT_MAX_USAGE];
};

struct bt_data {
    /* Bi-directional */
    atomic_t flags;
    /* from adapter */
    uint8_t output[64];
    /* from wireless */
    int32_t dev_id;
    int32_t dev_type;
    uint32_t report_id;
    uint32_t report_cnt;
    int32_t report_type;
    struct hid_report reports[REPORT_MAX];
    uint8_t input[128];
    int32_t axes_cal[ADAPTER_MAX_AXES];
    uint32_t sdp_len;
    uint8_t sdp_data[2048];
} __packed;

struct wired_data {
    void *fb_timer_hdl;
    /* Bi-directional */
    atomic_t flags;
    /* from wired driver */
    uint32_t frame_cnt;
    /* from adapter */
    int32_t dev_mode;
    int32_t acc_mode;
    uint8_t output[64];
} __packed;

struct wired_adapter {
    /* from wired driver */
    int32_t system_id;
    void *input_q_hdl;
    /* from adapter */
    int32_t driver_mode;
    /* Bi-directional */
    struct wired_data data[WIRED_MAX_DEV];
};

struct bt_adapter {
    struct bt_data data[BT_MAX_DEV];
};

typedef void (*to_generic_t)(struct bt_data *bt_data, struct generic_ctrl *ctrl_data);
typedef void (*from_generic_t)(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data);
typedef void (*fb_to_generic_t)(int32_t dev_mode, uint8_t *raw_fb_data, uint32_t raw_fb_len, struct generic_fb *fb_data);
typedef void (*fb_from_generic_t)(struct generic_fb *fb_data, struct bt_data *bt_data);
typedef void (*meta_init_t)(int32_t dev_mode, struct generic_ctrl *ctrl_data);
typedef void (*buffer_init_t)(int32_t dev_mode, struct wired_data *wired_data);

extern const uint32_t hat_to_ld_btns[16];
extern const uint32_t generic_btns_mask[32];
extern struct bt_adapter bt_adapter;
extern struct wired_adapter wired_adapter;

uint8_t btn_id_to_axis(uint8_t btn_id);
uint32_t axis_to_btn_mask(uint8_t axis);
int8_t btn_sign(uint32_t polarity, uint8_t btn_id);
void adapter_init_buffer(uint8_t wired_id);
void adapter_bridge(struct bt_data *bt_data);
void adapter_fb_stop_timer_start(uint8_t dev_id, uint64_t dur_us);
void adapter_fb_stop_timer_stop(uint8_t dev_id);
uint32_t adapter_bridge_fb(uint8_t *fb_data, uint32_t fb_len, struct bt_data *bt_data);
void IRAM_ATTR adapter_q_fb(uint8_t *data, uint32_t len);
void adapter_init(void);

#endif /* _ADAPTER_H_ */
