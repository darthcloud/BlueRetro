#include "io.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <esp_private/esp_timer_impl.h>
#include <driver/timer.h>
#include "zephyr/atomic.h"

typedef void (*convert_generic_func_t)(uint8_t*, struct io*, struct generic_map*);

enum {
    /* The rumble motor should be on. */
    IO_RUMBLE_MOTOR_ON,

    /* Flag for detecting when all buttons are released. */
    IO_NO_BTNS_PRESSED,

    /* Flag to force reporting emptied slot. */

    IO_FORCE_EMPTIED,

    /* Flag to control menu rumble feedback. */
    IO_RUMBLE_FEEDBACK,

    /* Is the GC controller a WaveBird? */
    IO_CTRL_PENDING_INIT,

    /* WaveBird association state. */
    IO_CTRL_READY,

    /* Adaptor bypass mode. */
    IO_BYPASS_MODE,

    /* Joystick calibrated. */
    IO_CALIBRATED,

    /* Flag for tracking if remap source is an axis. */
    IO_AXIS,

    /* Flag for button layout modification. */
    IO_LAYOUT_MODIFIER,

    /* Flag for N64 CTRL2. */
    IO_CTRL2,

    /* Set when we want to remap an analog trigger. */
    IO_TRIGGER,

    /* Joystick menu flags. */
    IO_CS,
    IO_AXIS_Y,

    /* 1st controller mute flag. */
    IO_MUTE,

    /* Menu multi-level option flags. */
    IO_MODE,
    IO_LAYOUT,
    IO_JOYSTICK,
    IO_PRESET,
    IO_REMAP,
    IO_SPECIAL,

    /* We're waiting for a button to be released, cleared when no buttons are pressed. */
    IO_WAITING_FOR_RELEASE,

    /* Menu levels flags. */
    IO_MENU_LEVEL1,
    IO_MENU_LEVEL2,
    IO_MENU_LEVEL3,
};

const uint8_t map_presets[8][32] =
{
    {
        /*DU*/BTN_DU, /*DL*/BTN_DL, /*DR*/BTN_DR, /*DD*/BTN_DD, /*LU*/BTN_LU, /*LL*/BTN_LL, /*LR*/BTN_LR, /*LD*/BTN_LD,
        /*BU*/BTN_RL, /*BL*/BTN_BL, /*BR*/BTN_RD, /*BD*/BTN_BD, /*RU*/BTN_RU, /*RL*/BTN_RL, /*RR*/BTN_RR, /*RD*/BTN_RD,
        /*LA*/BTN_NN, /*LM*/BTN_LM, /*RA*/BTN_NN, /*RM*/BTN_RM, /*LS*/BTN_LS, /*LG*/BTN_NN, /*LJ*/BTN_NN, /*RS*/BTN_LS,
        /*RG*/BTN_NN, /*RJ*/BTN_NN, /*SL*/BTN_SL, /*HM*/BTN_HM, /*ST*/BTN_ST, /*BE*/BTN_BE, BTN_NN, BTN_NN
    },
    {
        /*DU*/BTN_DU, /*DL*/BTN_DL, /*DR*/BTN_DR, /*DD*/BTN_DD, /*LU*/BTN_RU, /*LL*/BTN_RL, /*LR*/BTN_RR, /*LD*/BTN_RD,
        /*BU*/BTN_RL, /*BL*/BTN_BL, /*BR*/BTN_RD, /*BD*/BTN_BD, /*RU*/BTN_LU, /*RL*/BTN_LL, /*RR*/BTN_LR, /*RD*/BTN_LD,
        /*LA*/BTN_NN, /*LM*/BTN_LM, /*RA*/BTN_NN, /*RM*/BTN_RM, /*LS*/BTN_LS, /*LG*/BTN_NN, /*LJ*/BTN_NN, /*RS*/BTN_LS,
        /*RG*/BTN_NN, /*RJ*/BTN_NN, /*SL*/BTN_SL, /*HM*/BTN_HM, /*ST*/BTN_ST, /*BE*/BTN_BE, BTN_NN, BTN_NN
    },
    {
        /*DU*/BTN_DU, /*DL*/BTN_DL, /*DR*/BTN_DR, /*DD*/BTN_DD, /*LU*/BTN_LU, /*LL*/BTN_LL, /*LR*/BTN_LR, /*LD*/BTN_LD,
        /*BU*/BTN_RL, /*BL*/BTN_BL, /*BR*/BTN_RD, /*BD*/BTN_BD, /*RU*/BTN_RU, /*RL*/BTN_RL, /*RR*/BTN_RR, /*RD*/BTN_RD,
        /*LA*/BTN_NN, /*LM*/BTN_LM, /*RA*/BTN_NN, /*RM*/BTN_RM, /*LS*/BTN_LS, /*LG*/BTN_NN, /*LJ*/BTN_NN, /*RS*/BTN_LS,
        /*RG*/BTN_NN, /*RJ*/BTN_NN, /*SL*/BTN_SL, /*HM*/BTN_HM, /*ST*/BTN_ST, /*BE*/BTN_BE, BTN_NN, BTN_NN
    },
    {
        /*DU*/BTN_DU, /*DL*/BTN_DL, /*DR*/BTN_DR, /*DD*/BTN_DD, /*LU*/BTN_LU, /*LL*/BTN_LL, /*LR*/BTN_LR, /*LD*/BTN_LD,
        /*BU*/BTN_RL, /*BL*/BTN_BL, /*BR*/BTN_RD, /*BD*/BTN_BD, /*RU*/BTN_RU, /*RL*/BTN_RL, /*RR*/BTN_RR, /*RD*/BTN_RD,
        /*LA*/BTN_NN, /*LM*/BTN_LM, /*RA*/BTN_NN, /*RM*/BTN_RM, /*LS*/BTN_LS, /*LG*/BTN_NN, /*LJ*/BTN_NN, /*RS*/BTN_LS,
        /*RG*/BTN_NN, /*RJ*/BTN_NN, /*SL*/BTN_SL, /*HM*/BTN_HM, /*ST*/BTN_ST, /*BE*/BTN_BE, BTN_NN, BTN_NN
    },
    {
        /*DU*/BTN_DU, /*DL*/BTN_DL, /*DR*/BTN_DR, /*DD*/BTN_DD, /*LU*/BTN_LU, /*LL*/BTN_LL, /*LR*/BTN_LR, /*LD*/BTN_LD,
        /*BU*/BTN_RL, /*BL*/BTN_BL, /*BR*/BTN_RD, /*BD*/BTN_BD, /*RU*/BTN_RU, /*RL*/BTN_RL, /*RR*/BTN_RR, /*RD*/BTN_RD,
        /*LA*/BTN_NN, /*LM*/BTN_LM, /*RA*/BTN_NN, /*RM*/BTN_RM, /*LS*/BTN_LS, /*LG*/BTN_NN, /*LJ*/BTN_NN, /*RS*/BTN_LS,
        /*RG*/BTN_NN, /*RJ*/BTN_NN, /*SL*/BTN_SL, /*HM*/BTN_HM, /*ST*/BTN_ST, /*BE*/BTN_BE, BTN_NN, BTN_NN
    },
    {
        /*DU*/BTN_DU, /*DL*/BTN_DL, /*DR*/BTN_DR, /*DD*/BTN_DD, /*LU*/BTN_LU, /*LL*/BTN_LL, /*LR*/BTN_LR, /*LD*/BTN_LD,
        /*BU*/BTN_RL, /*BL*/BTN_BL, /*BR*/BTN_RD, /*BD*/BTN_BD, /*RU*/BTN_RU, /*RL*/BTN_RL, /*RR*/BTN_RR, /*RD*/BTN_RD,
        /*LA*/BTN_NN, /*LM*/BTN_LM, /*RA*/BTN_NN, /*RM*/BTN_RM, /*LS*/BTN_LS, /*LG*/BTN_NN, /*LJ*/BTN_NN, /*RS*/BTN_LS,
        /*RG*/BTN_NN, /*RJ*/BTN_NN, /*SL*/BTN_SL, /*HM*/BTN_HM, /*ST*/BTN_ST, /*BE*/BTN_BE, BTN_NN, BTN_NN
    },
    {
        /*DU*/BTN_DU, /*DL*/BTN_DL, /*DR*/BTN_DR, /*DD*/BTN_DD, /*LU*/BTN_LU, /*LL*/BTN_LL, /*LR*/BTN_LR, /*LD*/BTN_LD,
        /*BU*/BTN_RL, /*BL*/BTN_BL, /*BR*/BTN_RD, /*BD*/BTN_BD, /*RU*/BTN_RU, /*RL*/BTN_RL, /*RR*/BTN_RR, /*RD*/BTN_RD,
        /*LA*/BTN_NN, /*LM*/BTN_LM, /*RA*/BTN_NN, /*RM*/BTN_RM, /*LS*/BTN_LS, /*LG*/BTN_NN, /*LJ*/BTN_NN, /*RS*/BTN_LS,
        /*RG*/BTN_NN, /*RJ*/BTN_NN, /*SL*/BTN_SL, /*HM*/BTN_HM, /*ST*/BTN_ST, /*BE*/BTN_BE, BTN_NN, BTN_NN
    },
    {
        /*DU*/BTN_DU, /*DL*/BTN_DL, /*DR*/BTN_DR, /*DD*/BTN_DD, /*LU*/BTN_LU, /*LL*/BTN_LL, /*LR*/BTN_LR, /*LD*/BTN_LD,
        /*BU*/BTN_RL, /*BL*/BTN_BL, /*BR*/BTN_RD, /*BD*/BTN_BD, /*RU*/BTN_RU, /*RL*/BTN_RL, /*RR*/BTN_RR, /*RD*/BTN_RD,
        /*LA*/BTN_NN, /*LM*/BTN_LM, /*RA*/BTN_NN, /*RM*/BTN_RM, /*LS*/BTN_LS, /*LG*/BTN_NN, /*LJ*/BTN_NN, /*RS*/BTN_LS,
        /*RG*/BTN_NN, /*RJ*/BTN_NN, /*SL*/BTN_SL, /*HM*/BTN_HM, /*ST*/BTN_ST, /*BE*/BTN_BE, BTN_NN, BTN_NN
    },
};

const uint32_t generic_mask[32] =
{
/*  DU              DL              DR              DD               LU             LL              LR              LD               */
    (1U << BTN_DU), (1U << BTN_DL), (1U << BTN_DR), (1U << BTN_DD), (1U << BTN_LU), (1U << BTN_LL), (1U << BTN_LR), (1U << BTN_LD),
/*  BU              BL              BR              BD               RU             RL              RR              RD               */
    (1U << BTN_BU), (1U << BTN_BL), (1U << BTN_BR), (1U << BTN_BD), (1U << BTN_RU), (1U << BTN_RL), (1U << BTN_RR), (1U << BTN_RD),
/*  LA              LM              RA              RM               LS             LG              LJ              RS               */
    (1U << BTN_LA), (1U << BTN_LM), (1U << BTN_RA), (1U << BTN_RM), (1U << BTN_LS), (1U << BTN_LG), (1U << BTN_LJ), (1U << BTN_RS),
/*  RG              RJ              SL              HM               ST             BE                                               */
    (1U << BTN_RG), (1U << BTN_RJ), (1U << BTN_SL), (1U << BTN_HM), (1U << BTN_ST), (1U << BTN_BE), 0, 0
};

const uint8_t generic_axes_idx[6] =
{
    AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R
};

const uint32_t nes_mask[32] =
{
/*  DU    DL    DR    DD    LU    LL    LR    LD    BU    BL    BR    BD    RU    RL    RR    RD    */
    0x08, 0x02, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x10, 0x00, 0x00, 0x00
/*  LA    LM    RA    RM    LS    LG    LJ    RS    RG    RJ    SL    HM    ST    BE                */
};

const uint32_t snes_mask[32] =
{
/*  DU      DL      DR      DD      LU      LL      LR      LD      BU      BL      BR      BD      RU      RL      RR      RD      */
    0x0008, 0x0002, 0x0001, 0x0004, 0x0000, 0x0000, 0x0000, 0x0000, 0x4000, 0x0040, 0x8000, 0x0080, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x2000, 0x0000, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0000, 0x0010, 0x0000, 0x0000, 0x0000
/*  LA      LM      RA      RM      LS      LG      LJ      RS      RG      RJ      SL      HM      ST      BE                      */
};

const uint32_t n64_mask[32] =
{
/*  DU      DL      DR      DD      LU      LL      LR      LD      BU      BL      BR      BD      RU      RL      RR      RD      */
    0x0008, 0x0002, 0x0001, 0x0004, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0040, 0x0000, 0x0080, 0x0800, 0x0200, 0x0100, 0x0400,
    0x0000, 0x2000, 0x0000, 0x1000, 0x0020, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0010, 0x0000, 0x0000, 0x0000
/*  LA      LM      RA      RM      LS      LG      LJ      RS      RG      RJ      SL      HM      ST      BE                      */
};
const uint8_t n64_axes_idx[6] =
{
    AXIS_LX, AXIS_LY
};


const uint32_t wiiu_mask[32] =
{
/*  DU       DL       DR       DD       LU       LL       LR       LD       BU       BL       BR       BD       RU       RL       RR       RD       */
    0x00100, 0x00200, 0x00080, 0x00040, 0x00000, 0x00000, 0x00000, 0x00000, 0x00800, 0x02000, 0x01000, 0x04000, 0x00000, 0x00000, 0x00000, 0x00000,
    0x00000, 0x00020, 0x00000, 0x00002, 0x08000, 0x00000, 0x20000, 0x00400, 0x00000, 0x10000, 0x00010, 0x00008, 0x00004, 0x00000, 0x00000, 0x00000
/*  LA       LM       RA       RM       LS       LG       LJ       RS       RG       RJ       SL       HM       ST       BE                         */
};
const uint8_t wiiu_axes_idx[6] =
{
    AXIS_LX, AXIS_RX, AXIS_LY, AXIS_RY
};

const uint8_t axes_to_btn_mask_p[6] =
{
    BTN_LR, BTN_LU, BTN_RR, BTN_RU, BTN_LA, BTN_RA
};

const uint8_t axes_to_btn_mask_n[6] =
{
    BTN_LL, BTN_LD, BTN_RL, BTN_RD, BTN_NN, BTN_NN
};

static uint8_t btn_mask_to_axis(uint8_t btn_mask) {
    switch (btn_mask) {
        case BTN_LR:
        case BTN_LL:
            return AXIS_LX;
        case BTN_LU:
        case BTN_LD:
            return AXIS_LY;
        case BTN_RR:
        case BTN_RL:
            return AXIS_RX;
        case BTN_RU:
        case BTN_RD:
            return AXIS_RY;
        case BTN_LA:
            return TRIG_L;
        case BTN_RA:
            return TRIG_R;
    }
    return 0xFF;
}

static int8_t btn_mask_sign(uint8_t btn_mask) {
    switch (btn_mask) {
        case BTN_LR:
        case BTN_LU:
        case BTN_RR:
        case BTN_RU:
        case BTN_LA:
        case BTN_RA:
            return 1;
        case BTN_LL:
        case BTN_LD:
        case BTN_RL:
        case BTN_RD:
            return -1;
    }
    return 0;
}

const struct axis_meta n64_axes_meta =
{
    .abs_max = 0x54,
    .sign = 1,
};

const struct axis_meta wiiu_pro_axes_meta =
{
    .neutral = 0x800,
    .deadzone = 0x00F,
    .abs_btn_thrs = 0x250,
    .abs_max = 0x44C,
};

static atomic_t io_flags = 0;
static uint8_t leds_rumble = 0x10;
static int32_t axis_cal[6] = {0};

static int in_menu(void) {
    return (io_flags & ((1U << IO_MENU_LEVEL1) | (1U << IO_MENU_LEVEL2) | (1U << IO_MENU_LEVEL3))) ? 1 : 0;
}

static void wiiu_pro_to_generic(uint8_t map_table[], struct io *specific, struct generic_map *generic) {
    uint8_t i;

    for (i = 0; i < 30; i++) {
        if (~specific->io.wiiu_pro.buttons & wiiu_mask[i]) {
            generic->buttons |= generic_mask[i];
        }
    }
    for (i = 0; i < sizeof(specific->io.wiiu_pro.axes)/sizeof(*specific->io.wiiu_pro.axes); i++) {
        generic->axes[i].meta = &wiiu_pro_axes_meta;
        generic->axes[i].value = specific->io.wiiu_pro.axes[wiiu_axes_idx[i]] - wiiu_pro_axes_meta.neutral;
        if (generic->axes[i].value > generic->axes[i].meta->abs_btn_thrs) {
            generic->buttons |= generic_mask[axes_to_btn_mask_p[i]];
        }
        else if (generic->axes[i].value < -generic->axes[i].meta->abs_btn_thrs) {
            generic->buttons |= generic_mask[axes_to_btn_mask_n[i]];
        }
    }
}

const convert_generic_func_t convert_to_generic_func[16] =
{
    NULL, /* Generic */
    NULL, /* NES */
    NULL, /* SNES */
    NULL, /* N64 */
    NULL, /* GC */
    NULL, /* Wii */
    wiiu_pro_to_generic
};

static void n64_from_generic(uint8_t map_table[], struct io *specific, struct generic_map *generic) {
    uint8_t i;
    int8_t axis_int;

    memset(&specific->io.n64, 0, sizeof(specific->io.n64));

    if (in_menu()) {
        return;
    }

    for (i = 0; i < sizeof(generic->axes)/sizeof(*generic->axes); i++) {
        if (generic->axes[i].meta) {
            axis_int = (int8_t)((float)generic->axes[i].value * ((float)n64_axes_meta.abs_max / (float)(generic->axes[i].meta->abs_max - generic->axes[i].meta->deadzone)));
            if (generic->axes[i].value < 0) {
                if (btn_mask_to_axis(map_table[axes_to_btn_mask_n[i]]) == AXIS_LX || btn_mask_to_axis(map_table[axes_to_btn_mask_n[i]]) == AXIS_LY) {
                    if (abs(axis_int) > abs(specific->io.n64.axes[btn_mask_to_axis(map_table[axes_to_btn_mask_n[i]])])) {
                        specific->io.n64.axes[btn_mask_to_axis(map_table[axes_to_btn_mask_n[i]])] = btn_mask_sign(map_table[axes_to_btn_mask_n[i]]) * -axis_int;
                    }
                }
            }
            else {
                if (btn_mask_to_axis(map_table[axes_to_btn_mask_p[i]]) == AXIS_LX || btn_mask_to_axis(map_table[axes_to_btn_mask_p[i]]) == AXIS_LY) {
                    if (abs(axis_int) > abs(specific->io.n64.axes[btn_mask_to_axis(map_table[axes_to_btn_mask_p[i]])])) {
                        specific->io.n64.axes[btn_mask_to_axis(map_table[axes_to_btn_mask_p[i]])] = btn_mask_sign(map_table[axes_to_btn_mask_p[i]]) * axis_int;
                    }
                }
            }
        }
    }

    /* Map buttons to */
    for (i = 0; i < 32; i++) {
        if (generic->buttons & generic_mask[i]) {
            specific->io.n64.buttons |= n64_mask[map_table[i]];
            if (btn_mask_sign(i) == 0 && (btn_mask_to_axis(map_table[i]) == AXIS_LX || btn_mask_to_axis(map_table[i]) == AXIS_LY)) {
                if (abs(n64_axes_meta.abs_max) > abs(specific->io.n64.axes[btn_mask_to_axis(map_table[i])])) {
                    specific->io.n64.axes[btn_mask_to_axis(map_table[i])] = btn_mask_sign(map_table[i]) * n64_axes_meta.abs_max;
                }
            }
        }
    }
}

const convert_generic_func_t convert_from_generic_func[16] =
{
    NULL, /* Generic */
    NULL, /* NES */
    NULL, /* SNES */
    n64_from_generic, /* N64 */
    NULL, /* GC */
    NULL, /* Wii */
    NULL
};

static inline void set_calibration(int32_t *var, struct axis *axis)
{
    *var = -axis->value;
}

static inline void apply_calibration(int32_t cal, struct axis *axis) {
    /* no clamping, controller really bad if required */
    axis->value += cal;
}

static inline void apply_deadzone(struct axis *axis) {
    if (axis->value > axis->meta->deadzone) {
        axis->value -= axis->meta->deadzone;
    }
    else if (axis->value < -axis->meta->deadzone) {
        axis->value += axis->meta->deadzone;
    }
    else {
        axis->value = 0;
    }
}

#define TIMER_DIVIDER 16
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER)
static void leds_flash_timer_start(float interval_sec)
{
    timer_config_t config = {
        .divider = 16,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .intr_type = TIMER_INTR_LEVEL,
        .auto_reload = TIMER_AUTORELOAD_EN
    };

    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, interval_sec * TIMER_SCALE);
    timer_start(TIMER_GROUP_0, TIMER_0);
}

static int leds_flash_timer_alert(void) {
    int sts;
    if ((sts = (TIMERG0.int_st_timers.val & 0x01) ? 1 : 0)) {
        TIMERG0.int_clr_timers.t0 = 1;
        TIMERG0.hw_timer[TIMER_0].config.alarm_en = TIMER_ALARM_EN;
    }
    return sts;
}

static void rumble_timer_start(float interval_sec)
{
    timer_config_t config = {
        .divider = 16,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .intr_type = TIMER_INTR_LEVEL,
        .auto_reload = TIMER_AUTORELOAD_DIS
    };

    timer_init(TIMER_GROUP_0, TIMER_1, &config);
    timer_enable_intr(TIMER_GROUP_0, TIMER_1);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_1, 0);
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_1, interval_sec * TIMER_SCALE);
    timer_start(TIMER_GROUP_0, TIMER_1);
}

static int rumble_timer_alert(void) {
    int sts;
    if ((sts = (TIMERG0.int_st_timers.val & 0x02) ? 1 : 0)) {
        TIMERG0.int_clr_timers.t1 = 1;
    }
    return sts;
}

static void update_leds_rumble(struct io *input, struct io *output) {
    uint8_t old_leds_rumble = leds_rumble;

    if (atomic_test_bit(&output->flags, WRIO_RUMBLE_ON) || atomic_test_bit(&io_flags, IO_RUMBLE_FEEDBACK)) {
        atomic_set_bit(&io_flags, IO_RUMBLE_MOTOR_ON);
    }
    else {
        atomic_clear_bit(&io_flags, IO_RUMBLE_MOTOR_ON);
    }

    if (atomic_test_bit(&io_flags, IO_MENU_LEVEL1)) {
        leds_rumble &= 0x11;
        if (leds_flash_timer_alert()) {
            leds_rumble ^= 0x10;
        }
    }
    else if (atomic_test_bit(&io_flags, IO_MENU_LEVEL2)) {
        leds_rumble &= 0x21;
        if (leds_flash_timer_alert()) {
            leds_rumble ^= 0x20;
        }
    }
    else if (atomic_test_bit(&io_flags, IO_MENU_LEVEL3)) {
        leds_rumble &= 0x41;
        if (leds_flash_timer_alert()) {
            leds_rumble ^= 0x40;
        }
    }
    else {
        leds_rumble &= 0x01;
        leds_rumble |= 0x10;
    }

    if (atomic_test_bit(&io_flags, IO_RUMBLE_MOTOR_ON)) {
        if (rumble_timer_alert()) {
            atomic_clear_bit(&io_flags, IO_RUMBLE_FEEDBACK);
            leds_rumble &= 0xFE;
        }
        else {
            leds_rumble |= 0x01;
        }
    }
    else {
        leds_rumble &= 0xFE;
    }

    if (old_leds_rumble != leds_rumble) {
        atomic_set_bit(&input->flags, BTIO_UPDATE_CTRL);
        input->leds_rumble = leds_rumble;
    }
}

static void menu(struct generic_map *input)
{
    if (atomic_test_bit(&io_flags, IO_MENU_LEVEL1)) {
        if (input->buttons) {
            atomic_clear_bit(&io_flags, IO_MENU_LEVEL1);
            atomic_set_bit(&io_flags, IO_MENU_LEVEL2);
            atomic_set_bit(&io_flags, IO_WAITING_FOR_RELEASE);
            atomic_set_bit(&io_flags, IO_RUMBLE_FEEDBACK);
            rumble_timer_start(0.3);
            printf("JG2019 In Menu 2\n");
        }
    }
    else if (atomic_test_bit(&io_flags, IO_MENU_LEVEL2)) {
        if (input->buttons) {
            atomic_clear_bit(&io_flags, IO_MENU_LEVEL2);
            atomic_set_bit(&io_flags, IO_MENU_LEVEL3);
            atomic_set_bit(&io_flags, IO_WAITING_FOR_RELEASE);
            atomic_set_bit(&io_flags, IO_RUMBLE_FEEDBACK);
            rumble_timer_start(0.3);
            printf("JG2019 In Menu 3\n");
        }
    }
    else if (atomic_test_bit(&io_flags, IO_MENU_LEVEL3)) {
        if (input->buttons) {
            atomic_clear_bit(&io_flags, IO_MENU_LEVEL3);
            atomic_set_bit(&io_flags, IO_WAITING_FOR_RELEASE);
            atomic_set_bit(&io_flags, IO_RUMBLE_FEEDBACK);
            rumble_timer_start(0.3);
            printf("JG2019 Menu exit\n");
        }
    }
    else if (input->buttons & generic_mask[BTN_HM]) {
        atomic_set_bit(&io_flags, IO_WAITING_FOR_RELEASE);
        atomic_set_bit(&io_flags, IO_MENU_LEVEL1);
        leds_flash_timer_start(0.3);
        atomic_set_bit(&io_flags, IO_RUMBLE_FEEDBACK);
        rumble_timer_start(0.3);
        printf("JG2019 In Menu 1\n");
    }
}

void translate_status(struct config *config, struct io *input, struct io* output) {
    struct generic_map generic = {0};
    uint8_t i;
    uint64_t start_us, end_us;

    start_us = esp_timer_impl_get_time();

    convert_to_generic_func[input->format](NULL, input, &generic);

    if (!generic.buttons) {
        atomic_clear_bit(&io_flags, IO_WAITING_FOR_RELEASE);
    }

    if (!atomic_test_bit(&io_flags, IO_CALIBRATED)) {
        /* Init calib */
        for (i = 0; i < sizeof(generic.axes)/sizeof(*generic.axes); i++) {
            set_calibration(&axis_cal[i], &generic.axes[i]);
        }
        atomic_set_bit(&io_flags, IO_CALIBRATED);
    }

    /* Apply calib */
    for (i = 0; i < sizeof(generic.axes)/sizeof(*generic.axes); i++) {
        apply_calibration(axis_cal[i], &generic.axes[i]);
    }

    /* Apply deadzone */
    for (i = 0; i < sizeof(generic.axes)/sizeof(*generic.axes); i++) {
        if (generic.axes[i].meta) {
            apply_deadzone(&generic.axes[i]);
        }
    }

    update_leds_rumble(input, output);

    /* Execute menu if Home buttons pressed */
    if (!atomic_test_bit(&io_flags, IO_WAITING_FOR_RELEASE)) {
        menu(&generic);
    }

    if (output->format) {
        convert_from_generic_func[output->format](config->mapping[config->set_map], output, &generic);
    }
    end_us = esp_timer_impl_get_time();
    //printf("%llu us\n", (end_us - start_us));
}

