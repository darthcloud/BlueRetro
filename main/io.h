#ifndef _IO_H_
#define _IO_H_

#include <sys/cdefs.h>
#include <stdint.h>

#define BTN_D_UP     0x00
#define BTN_D_LEFT   0x01
#define BTN_D_RIGHT  0x02
#define BTN_D_DOWN   0x03
#define BTN_LJ_UP    0x04
#define BTN_LJ_LEFT  0x05
#define BTN_LJ_RIGHT 0x06
#define BTN_LJ_DOWN  0x07
#define BTN_RJ_UP    0x08
#define BTN_RJ_LEFT  0x09
#define BTN_RJ_RIGHT 0x0A
#define BTN_RJ_DOWN  0x0B

/*
 * Setup analog triggers so their LSB is clear since they are positive axes 
 * and make sure both analog trigger has only 1 bit different (2nd bit).
 */

#define BTN_LA       0x0C
#define BTN_L        0x0D
#define BTN_RA       0x0E
#define BTN_R        0x0F
#define BTN_LZ       0x10
#define BTN_LG       0x11
#define BTN_LJ       0x12
#define BTN_RZ       0x13
#define BTN_RG       0x14
#define BTN_RJ       0x15

#define BTN_A        0x16
#define BTN_B        0x17
#define BTN_X        0x18
#define BTN_Y        0x19

#define BTN_SELECT   0x1A
#define BTN_HOME     0x1B
#define BTN_START    0x1C
#define BTN_C        0x1D
#define BTN_MAX      0x1E
#define BTN_NONE     0x1F

#define IO_FORMAT_NES      0x00
struct nes_map {
    uint8_t buttons;
} __packed;
extern const uint8_t nes_mask[32];

#define IO_FORMAT_SNES     0x01
struct snes_map {
    uint16_t buttons;
} __packed;
extern const uint16_t snes_mask[32];

#define IO_FORMAT_N64      0x02
struct n64_map {
    uint16_t buttons;
    int8_t ls_x_axis;
    int8_t ls_y_axis;
} __packed;
extern const uint16_t n64_mask[32];

#define IO_FORMAT_GC       0x04
struct gc_map {
    uint16_t buttons;
    uint8_t ls_x_axis;
    uint8_t ls_y_axis;
    uint8_t rs_x_axis;
    uint8_t rs_y_axis;
    uint8_t la_axis;
    uint8_t ra_axis;
} __packed;

#define IO_FORMAT_WIIU_PRO 0x05
struct wiiu_pro_map {
    uint16_t ls_x_axis;
    uint16_t rs_x_axis;
    uint16_t ls_y_axis;
    uint16_t rs_y_axis;
    uint32_t buttons;
} __packed;
extern const uint32_t wiiu_mask[32];

struct io {
    uint8_t format;
    union {
        struct nes_map nes;
        struct snes_map snes;
        struct n64_map n64;
        struct gc_map gc;
        struct wiiu_pro_map wiiu_pro;
    } io;
} __packed;

void translate_status(struct io *input, struct io* output);

#endif /* _IO_H_ */

