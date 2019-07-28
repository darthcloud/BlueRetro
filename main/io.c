#include "io.h"

#include <stdio.h>

const uint8_t nes_mask[32] =
{
/*  DU    DL    DR    DD    LJU   LJL   LJR   LJD   RJU   RJL   RJR   RJD   LA    L     RA    R     */
    0x08, 0x02, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x40, 0x00, 0x00, 0x20, 0x00, 0x10, 0x00, 0x00, 0x00
/*  LZ    LG    LJ    RZ    RG    RJ    A     B     X     Y     SEL   HOME  STA   C                 */
};

const uint16_t snes_mask[32] =
{
/*  DU      DL      DR      DD      LJU     LJL     LJR     LJD     RJU     RJL     RJR     RJD     LA      L       RA      R       */
    0x0008, 0x0002, 0x0001, 0x0004, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2000, 0x0000, 0x1000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8000, 0x0080, 0x4000, 0x0040, 0x0020, 0x0000, 0x0010, 0x0000, 0x0000, 0x0000
/*  LZ      LG      LJ      RZ      RG      RJ      A       B       X       Y       SEL     HOME    STA     C                       */
};

const uint16_t n64_mask[32] =
{
/*  DU      DL      DR      DD      LJU     LJL     LJR     LJD     RJU     RJL     RJR     RJD     LA      L       RA      R       */
    0x0008, 0x0002, 0x0001, 0x0004, 0x0000, 0x0000, 0x0000, 0x0000, 0x0800, 0x0200, 0x0100, 0x0400, 0x0000, 0x2000, 0x0000, 0x1000,
    0x0000, 0x0000, 0x0000, 0x0020, 0x0000, 0x0000, 0x0080, 0x0040, 0x0000, 0x0000, 0x0000, 0x0000, 0x0010, 0x0000, 0x0000, 0x0000
/*  LZ      LG      LJ      RZ      RG      RJ      A       B       X       Y       SEL     HOME    STA     C                       */
};

const uint32_t wiiu_mask[32] =
{
/*  DU       DL       DR       DD       LJU      LJL      LJR      LJD      RJU      RJL      RJR      RJD      LA       L        RA       R        */
    0x00100, 0x00200, 0x00080, 0x00040, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00020, 0x00000, 0x00002,
    0x00800, 0x00000, 0x20000, 0x00400, 0x00000, 0x10000, 0x01000, 0x04000, 0x00800, 0x02000, 0x00010, 0x00008, 0x00004, 0x00000, 0x00000, 0x00000
/*  LZ       LG       LJ       RZ       RG       RJ       A        B        X        Y        SEL      HOME     STA      C                          */
};

void translate_status(struct io *input, struct io* output) {
    output->io.n64.buttons = 0x0000;
    for (uint8_t i = 0; i < 32; i++) {
        if (~input->io.wiiu_pro.buttons & wiiu_mask[i]) {
            output->io.n64.buttons |= n64_mask[i];
        }
    }
    if (input->io.wiiu_pro.rs_x_axis > 0x900) {
        output->io.n64.buttons |= n64_mask[BTN_RJ_RIGHT];
    }
    else if (input->io.wiiu_pro.rs_x_axis < 0x700) {
        output->io.n64.buttons |= n64_mask[BTN_RJ_LEFT];
    }
    if (input->io.wiiu_pro.rs_y_axis > 0x900) {
        output->io.n64.buttons |= n64_mask[BTN_RJ_UP];
    }
    else if (input->io.wiiu_pro.rs_y_axis < 0x700) {
        output->io.n64.buttons |= n64_mask[BTN_RJ_DOWN];
    }
    output->io.n64.ls_x_axis = (input->io.wiiu_pro.ls_x_axis >> 4) - 0x80;
    output->io.n64.ls_y_axis = (input->io.wiiu_pro.ls_y_axis >> 4) - 0x80;
}
