#ifndef _IO_H_
#define _IO_H_

#include <sys/cdefs.h>
#include <stdint.h>

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

#define AXIS_BTN_THRS 0x20

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

