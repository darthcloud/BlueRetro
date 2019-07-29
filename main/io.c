#include "io.h"

#include <stdio.h>
#include "zephyr/atomic.h"

enum {
    IO_CALIBRATED
};

const uint8_t nes_mask[32] =
{
/*  DU    DL    DR    DD    LU    LL    LR    LD    BU    BL    BR    BD    RU    RL    RR    RD    */
    0x08, 0x02, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x10, 0x00, 0x00, 0x00
/*  LA    LM    RA    RM    LS    LG    LJ    RS    RG    RJ    SL    HM    ST    BE                */
};

const uint16_t snes_mask[32] =
{
/*  DU      DL      DR      DD      LU      LL      LR      LD      BU      BL      BR      BD      RU      RL      RR      RD      */
    0x0008, 0x0002, 0x0001, 0x0004, 0x0000, 0x0000, 0x0000, 0x0000, 0x4000, 0x0040, 0x8000, 0x0080, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x2000, 0x0000, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0000, 0x0010, 0x0000, 0x0000, 0x0000
/*  LA      LM      RA      RM      LS      LG      LJ      RS      RG      RJ      SL      HM      ST      BE                      */
};

const uint16_t n64_mask[32] =
{
/*  DU      DL      DR      DD      LU      LL      LR      LD      BU      BL      BR      BD      RU      RL      RR      RD      */
    0x0008, 0x0002, 0x0001, 0x0004, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0040, 0x0000, 0x0080, 0x0800, 0x0200, 0x0100, 0x0400,
    0x0000, 0x2000, 0x0000, 0x1000, 0x0020, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0010, 0x0000, 0x0000, 0x0000
/*  LA      LM      RA      RM      LS      LG      LJ      RS      RG      RJ      SL      HM      ST      BE                      */
};

const uint32_t wiiu_mask[32] =
{
/*  DU       DL       DR       DD       LU       LL       LR       LD       BU       BL       BR       BD       RU       RL       RR       RD       */
    0x00100, 0x00200, 0x00080, 0x00040, 0x00000, 0x00000, 0x00000, 0x00000, 0x00800, 0x02000, 0x01000, 0x04000, 0x00000, 0x00000, 0x00000, 0x00000,
    0x00000, 0x00020, 0x00000, 0x00002, 0x00800, 0x00000, 0x20000, 0x00400, 0x00000, 0x10000, 0x00010, 0x00008, 0x00004, 0x00000, 0x00000, 0x00000
/*  LA       LM       RA       RM       LS       LG       LJ       RS       RG       RJ       SL       HM       ST       BE                         */
};

static atomic_t io_flags = 0;
static uint8_t map_table[32] =
{
    /*DU*/BTN_DU, /*DL*/BTN_DL, /*DR*/BTN_DR, /*DD*/BTN_DD, /*LU*/BTN_LU, /*LL*/BTN_LL, /*LR*/BTN_LR, /*LD*/BTN_LD,
    /*BU*/BTN_RL, /*BL*/BTN_BL, /*BR*/BTN_RD, /*BD*/BTN_BD, /*RU*/BTN_RU, /*RL*/BTN_RL, /*RR*/BTN_RR, /*RD*/BTN_RD,
    /*LA*/BTN_NN, /*LM*/BTN_LM, /*RA*/BTN_NN, /*RM*/BTN_RM, /*LS*/BTN_LS, /*LG*/BTN_NN, /*LJ*/BTN_NN, /*RS*/BTN_LS,
    /*RG*/BTN_NN, /*RJ*/BTN_NN, /*SL*/BTN_SL, /*HM*/BTN_HM, /*ST*/BTN_ST, /*BE*/BTN_BE, BTN_NN, BTN_NN
};

static void map_axis_to_n64_axis(struct io* output, uint8_t btn_id, int8_t value) {
    switch (btn_id) {
        case BTN_LU:
            output->io.n64.ls_y_axis = value;
            break;
        case BTN_LD:
            output->io.n64.ls_y_axis = -value;
            break;
        case BTN_LL:
            output->io.n64.ls_x_axis = -value;
            break;
        case BTN_LR:
            output->io.n64.ls_x_axis = value;
            break;
    }
};

void translate_status(struct io *input, struct io* output) {
    int8_t scaled_lx, scaled_ly, scaled_rx, scaled_ry;

    /* Reset N64 status buffer */
    output->io.n64.buttons = 0x0000;
    output->io.n64.ls_x_axis = 0x00;
    output->io.n64.ls_y_axis = 0x00;

    if (!atomic_test_bit(&io_flags, IO_CALIBRATED)) {
        /* Store calib */
    }
    else{
        /* Apply calib */
    }

    /* Scale axis */
    scaled_lx = (input->io.wiiu_pro.ls_x_axis >> 4) - 0x80;
    scaled_ly = (input->io.wiiu_pro.ls_y_axis >> 4) - 0x80;
    scaled_rx = (input->io.wiiu_pro.rs_x_axis >> 4) - 0x80;
    scaled_ry = (input->io.wiiu_pro.rs_y_axis >> 4) - 0x80;

    /* Set responce curve */

    /* Map axis to */
    if (scaled_lx >= 0x0) {
        if (scaled_lx > 0x30) {
            output->io.n64.buttons |= n64_mask[map_table[BTN_LR]];
        }
        map_axis_to_n64_axis(output, map_table[BTN_LR], scaled_lx);
    }
    else {
        if (scaled_lx < -0x30) {
            output->io.n64.buttons |= n64_mask[map_table[BTN_LL]];
        }
        map_axis_to_n64_axis(output, map_table[BTN_LL], -scaled_lx);
    }
    if (scaled_ly >= 0x0) {
        if (scaled_ly > 0x30) {
            output->io.n64.buttons |= n64_mask[map_table[BTN_LU]];
        }
        map_axis_to_n64_axis(output, map_table[BTN_LU], scaled_ly);
    }
    else {
        if (scaled_ly < -0x30) {
            output->io.n64.buttons |= n64_mask[map_table[BTN_LD]];
        }
        map_axis_to_n64_axis(output, map_table[BTN_LD], -scaled_ly);
    }
    if (scaled_rx >= 0x0) {
        if (scaled_rx > 0x30) {
            output->io.n64.buttons |= n64_mask[map_table[BTN_RR]];
        }
        map_axis_to_n64_axis(output, map_table[BTN_RR], scaled_rx);
    }
    else {
        if (scaled_rx < -0x30) {
            output->io.n64.buttons |= n64_mask[map_table[BTN_RL]];
        }
        map_axis_to_n64_axis(output, map_table[BTN_RL], -scaled_rx);
    }
    if (scaled_ry >= 0x0) {
        if (scaled_ry > 0x30) {
            output->io.n64.buttons |= n64_mask[map_table[BTN_RU]];
        }
        map_axis_to_n64_axis(output, map_table[BTN_RU], scaled_ry);
    }
    else {
        if (scaled_ry < -0x30) {
            output->io.n64.buttons |= n64_mask[map_table[BTN_RD]];
        }
        map_axis_to_n64_axis(output, map_table[BTN_RD], -scaled_ry);
    }

    /* Map buttons to */
    for (uint8_t i = 0; i < 32; i++) {
        if (~input->io.wiiu_pro.buttons & wiiu_mask[i]) {
            output->io.n64.buttons |= n64_mask[map_table[i]];
            map_axis_to_n64_axis(output, map_table[i], 0x54);
        }
    }

}
