#include "io.h"

#include <stdio.h>
#include <stdlib.h>
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
    0x00000, 0x00020, 0x00000, 0x00002, 0x08000, 0x00000, 0x20000, 0x00400, 0x00000, 0x10000, 0x00010, 0x00008, 0x00004, 0x00000, 0x00000, 0x00000
/*  LA       LM       RA       RM       LS       LG       LJ       RS       RG       RJ       SL       HM       ST       BE                         */
};

static atomic_t io_flags = 0;
static int16_t lx_cal = 0;
static int16_t ly_cal = 0;
static int16_t rx_cal = 0;
static int16_t ry_cal = 0;

static uint8_t map_table[32] =
{
    /*DU*/BTN_DU, /*DL*/BTN_DL, /*DR*/BTN_DR, /*DD*/BTN_DD, /*LU*/BTN_LU, /*LL*/BTN_LL, /*LR*/BTN_LR, /*LD*/BTN_LD,
    /*BU*/BTN_RL, /*BL*/BTN_BL, /*BR*/BTN_RD, /*BD*/BTN_BD, /*RU*/BTN_RU, /*RL*/BTN_RL, /*RR*/BTN_RR, /*RD*/BTN_RD,
    /*LA*/BTN_NN, /*LM*/BTN_LM, /*RA*/BTN_NN, /*RM*/BTN_RM, /*LS*/BTN_LS, /*LG*/BTN_NN, /*LJ*/BTN_NN, /*RS*/BTN_LS,
    /*RG*/BTN_NN, /*RJ*/BTN_NN, /*SL*/BTN_SL, /*HM*/BTN_HM, /*ST*/BTN_ST, /*BE*/BTN_BE, BTN_NN, BTN_NN
};

static inline void set_calibration(int16_t *var, uint16_t val, uint16_t ideal)
{
    *var = ideal - val;
}

static inline void apply_calibration(uint16_t cal, uint16_t *val) {
    /* no clamping, controller really bad if required */
    *val += cal;
}

static inline void apply_deadzone(uint16_t *val) {
    if (*val >= AXIS_DEAD_ZONE) {
        *val -= AXIS_DEAD_ZONE;
    }
    else if (*val <= AXIS_DEAD_ZONE) {
        *val += AXIS_DEAD_ZONE;
    }
    else {
        *val = 0;
    }
}

static void map_to_n64_axis(struct io* output, uint8_t btn_id, int8_t value) {
    switch (btn_id) {
        case BTN_LU:
            if (abs(value) > abs(output->io.n64.ls_y_axis)) {
                output->io.n64.ls_y_axis = value;
            }
            break;
        case BTN_LD:
            if (abs(value) > abs(output->io.n64.ls_y_axis)) {
                output->io.n64.ls_y_axis = -value;
            }
            break;
        case BTN_LL:
            if (abs(value) > abs(output->io.n64.ls_x_axis)) {
                output->io.n64.ls_x_axis = -value;
            }
            break;
        case BTN_LR:
            if (abs(value) > abs(output->io.n64.ls_x_axis)) {
                output->io.n64.ls_x_axis = value;
            }
            break;
    }
};

static void map_axis_to_buttons_axis(struct io* output, uint8_t btn_n, uint8_t btn_p, int8_t value) {
    if (value >= 0x0) {
        if (value > AXIS_BTN_THRS) {
            output->io.n64.buttons |= n64_mask[map_table[btn_p]];
        }
        map_to_n64_axis(output, map_table[btn_p], value);
    }
    else {
        if (value < -AXIS_BTN_THRS) {
            output->io.n64.buttons |= n64_mask[map_table[btn_n]];
        }
        map_to_n64_axis(output, map_table[btn_n], -value);
    }
}

void translate_status(struct io *input, struct io* output) {
    int8_t scaled_lx, scaled_ly, scaled_rx, scaled_ry;

    /* Reset N64 status buffer */
    output->io.n64.buttons = 0x0000;
    output->io.n64.ls_x_axis = 0x00;
    output->io.n64.ls_y_axis = 0x00;

    if (!atomic_test_bit(&io_flags, IO_CALIBRATED)) {
        /* Init calib */
        set_calibration(&lx_cal, input->io.wiiu_pro.ls_x_axis, 0x800);
        set_calibration(&ly_cal, input->io.wiiu_pro.ls_y_axis, 0x800);
        set_calibration(&rx_cal, input->io.wiiu_pro.rs_x_axis, 0x800);
        set_calibration(&ry_cal, input->io.wiiu_pro.rs_y_axis, 0x800);
        atomic_set_bit(&io_flags, IO_CALIBRATED);
    }

    /* Apply calib */
    apply_calibration(lx_cal, &input->io.wiiu_pro.ls_x_axis);
    apply_calibration(ly_cal, &input->io.wiiu_pro.ls_y_axis);
    apply_calibration(rx_cal, &input->io.wiiu_pro.rs_x_axis);
    apply_calibration(ry_cal, &input->io.wiiu_pro.rs_y_axis);

    /* Apply deadzone */
    apply_deadzone(&input->io.wiiu_pro.ls_x_axis);
    apply_deadzone(&input->io.wiiu_pro.ls_y_axis);
    apply_deadzone(&input->io.wiiu_pro.rs_x_axis);
    apply_deadzone(&input->io.wiiu_pro.rs_y_axis);

    /* Scale axis */
    scaled_lx = (input->io.wiiu_pro.ls_x_axis >> 4) - 0x80;
    scaled_ly = (input->io.wiiu_pro.ls_y_axis >> 4) - 0x80;
    scaled_rx = (input->io.wiiu_pro.rs_x_axis >> 4) - 0x80;
    scaled_ry = (input->io.wiiu_pro.rs_y_axis >> 4) - 0x80;

    /* Set responce curve */

    /* Map axis to */
    map_axis_to_buttons_axis(output, BTN_LL, BTN_LR, scaled_lx);
    map_axis_to_buttons_axis(output, BTN_LD, BTN_LU, scaled_ly);
    map_axis_to_buttons_axis(output, BTN_RL, BTN_RR, scaled_rx);
    map_axis_to_buttons_axis(output, BTN_RD, BTN_RU, scaled_ry);

    /* Map buttons to */
    for (uint8_t i = 0; i < 32; i++) {
        if (~input->io.wiiu_pro.buttons & wiiu_mask[i]) {
            output->io.n64.buttons |= n64_mask[map_table[i]];
            map_to_n64_axis(output, map_table[i], 0x54);
        }
    }

}
