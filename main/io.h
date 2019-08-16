#ifndef _IO_H_
#define _IO_H_

#include <sys/cdefs.h>
#include <stdint.h>
#include "sd.h"

#define BTN_DU 0x00
#define BTN_DL 0x01
#define BTN_DR 0x02
#define BTN_DD 0x03
#define BTN_LU 0x04
#define BTN_LL 0x05
#define BTN_LR 0x06
#define BTN_LD 0x07
#define BTN_BU 0x08
#define BTN_BL 0x09
#define BTN_BR 0x0A
#define BTN_BD 0x0B
#define BTN_RU 0x0C
#define BTN_RL 0x0D
#define BTN_RR 0x0E
#define BTN_RD 0x0F

/*
 * Setup analog triggers so their LSB is clear since they are positive axes 
 * and make sure both analog trigger has only 1 bit different (2nd bit).
 */
#define BTN_LA 0x10
#define BTN_LM 0x11
#define BTN_RA 0x12
#define BTN_RM 0x13
#define BTN_LS 0x14
#define BTN_LG 0x15
#define BTN_LJ 0x16
#define BTN_RS 0x17
#define BTN_RG 0x18
#define BTN_RJ 0x19

#define BTN_SL 0x1A
#define BTN_HM 0x1B
#define BTN_ST 0x1C
#define BTN_BE 0x1D
#define BTN_MX 0x1E
#define BTN_NN 0x1F

#define AXIS_LX 0x00
#define AXIS_LY 0x01
#define AXIS_RX 0x02
#define AXIS_RY 0x03
#define TRIG_L  0x04
#define TRIG_R  0x05

#define WIIU_PRO_AXIS_SIGN     0
#define WIIU_PRO_AXIS_NEUTRAL  0x800
#define WIIU_PRO_AXIS_DEADZONE 0x00F
#define WIIU_PRO_AXIS_HALFWAY  0x200
#define WIIU_PRO_AXIS_MAX      0x490

enum {
    BTIO_UPDATE_CTRL
};

enum {
    WRIO_RUMBLE_ON,
    WRIO_SAVE_MEM
};

extern const uint8_t map_presets[8][32];

struct axis_meta {
    int32_t neutral;
    int32_t deadzone;
    int32_t abs_btn_thrs;
    int32_t abs_max;
    int32_t sign;
};

struct axis {
    int32_t value;
    const struct axis_meta *meta;
};

#define IO_FORMAT_GENERIC  0x00
struct generic_map {
    uint32_t buttons;
    struct axis axes[6];
};

#define IO_FORMAT_NES      0x01
struct nes_map {
    uint8_t buttons;
} __packed;

#define IO_FORMAT_SNES     0x02
struct snes_map {
    uint16_t buttons;
} __packed;

#define IO_FORMAT_N64      0x03
struct n64_map {
    uint16_t buttons;
    int8_t axes[2];
} __packed;

#define IO_FORMAT_GC       0x04
struct gc_map {
    uint16_t buttons;
    uint8_t axes[6];
} __packed;

#define IO_FORMAT_WIIU_PRO 0x06
struct wiiu_pro_map {
    uint16_t axes[4];
    uint32_t buttons;
} __packed;

struct io {
    int32_t flags;
    uint8_t format;
    uint8_t leds_rumble;
    union {
        struct nes_map nes;
        struct snes_map snes;
        struct n64_map n64;
        struct gc_map gc;
        struct wiiu_pro_map wiiu_pro;
    } io;
} __packed;

void translate_status(struct config *config, struct io *input, struct io* output);

#endif /* _IO_H_ */

