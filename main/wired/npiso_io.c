/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "system/intr.h"
#include "system/gpio.h"
#include "system/delay.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "npiso_io.h"

#define NPISO_PORT_MAX 2
#define NPISO_LATCH_PIN 32
#define NPISO_LATCH_MASK (1U << 0) /* First of 2nd bank of GPIO */
#define NPISO_TIMEOUT 4096

#define P1_CLK_PIN 5
#define P1_SEL_PIN 23
#define P1_D0_PIN 19
#define P1_D1_PIN 21
#define P2_CLK_PIN 18
#define P2_SEL_PIN 26
#define P2_D0_PIN 22
#define P2_D1_PIN 25

#define P1_CLK_MASK (1U << P1_CLK_PIN)
#define P1_SEL_MASK (1U << P1_SEL_PIN)
#define P1_D0_MASK (1U << P1_D0_PIN)
#define P1_D1_MASK (1U << P1_D1_PIN)
#define P2_CLK_MASK (1U << P2_CLK_PIN)
#define P2_SEL_MASK (1U << P2_SEL_PIN)
#define P2_D0_MASK (1U << P2_D0_PIN)
#define P2_D1_MASK (1U << P2_D1_PIN)

#define MOUSE_SPEED_MIN 0xFF
#define MOUSE_SPEED_MED 0xEF
#define MOUSE_SPEED_MAX 0xDF

#define VTAP_PAL_PIN 16
#define VTAP_MODE_PIN 27

enum {
    NPISO_CLK = 0,
    NPISO_SEL,
    NPISO_D0,
    NPISO_D1,
    NPISO_PIN_MAX,
};

enum {
    DEV_NONE = 0,
    DEV_FC_NES_PAD,
    DEV_FC_NES_MULTITAP,
    DEV_FC_MULTITAP_ALT,
    DEV_FC_TRACKBALL,
    DEV_FC_KB,
    DEV_SFC_SNES_PAD,
    DEV_SFC_SNES_MULTITAP,
    DEV_SFC_SNES_MOUSE,
    DEV_SNES_XBAND_KB,
};

struct counters {
    union {
        uint32_t val;
        struct {
            uint8_t cnt[4];
        };
        struct {
            uint16_t cnt_g0;
            uint16_t cnt_g1;
        };
    };
};

static const uint8_t gpio_pins[NPISO_PORT_MAX][NPISO_PIN_MAX] = {
    {P1_CLK_PIN, P1_SEL_PIN, P1_D0_PIN, P1_D1_PIN},
    {P2_CLK_PIN, P2_SEL_PIN, P2_D0_PIN, P2_D1_PIN},
};

static const uint32_t gpio_mask[NPISO_PORT_MAX][NPISO_PIN_MAX] = {
    {P1_CLK_MASK, P1_SEL_MASK, P1_D0_MASK, P1_D1_MASK},
    {P2_CLK_MASK, P2_SEL_MASK, P2_D0_MASK, P2_D1_MASK},
};

static uint8_t dev_type[NPISO_PORT_MAX] = {0};
static uint8_t mt_first_port[NPISO_PORT_MAX] = {0};
static uint32_t cnt[NPISO_PORT_MAX] = {0};
static uint32_t idx[NPISO_PORT_MAX];
static uint8_t mask[NPISO_PORT_MAX] = {0x80, 0x80};
static uint8_t fs_id[NPISO_PORT_MAX] = {0xEF, 0xDF};
static struct counters cnts = {0};
static uint8_t *cnt0 = &cnts.cnt[0];
static uint8_t *cnt1 = &cnts.cnt[1];
static uint32_t idx0, idx1;
static uint8_t mouse_speed[NPISO_PORT_MAX] = {MOUSE_SPEED_MIN, MOUSE_SPEED_MIN};
static uint8_t mouse_update[NPISO_PORT_MAX] = {0};
static uint8_t mouse_axes[NPISO_PORT_MAX][2] = {0};

static inline void set_data(uint8_t port, uint8_t data_id, uint8_t value) {
    uint32_t mask = gpio_mask[port][NPISO_D0 + data_id];

    if (value) {
        GPIO.out_w1ts = mask;
    }
    else {
        GPIO.out_w1tc = mask;
    }
}

static inline void load_mouse_axis(uint8_t port, uint8_t index) {
    uint8_t *relative = (uint8_t *)(wired_adapter.data[port].output + 2);
    int32_t *raw_axes = (int32_t *)(wired_adapter.data[port].output + 4);
    int32_t val = 0;

    if (relative[index]) {
        val = atomic_clear(&raw_axes[index]);
    }
    else {
        val = raw_axes[index];
    }

    if (val > 127) {
        mouse_axes[port][index] = 0x80;
    }
    else if (val < -128) {
        mouse_axes[port][index] = 0x00;
    }
    else {
        if (val < 0) {
            mouse_axes[port][index] = ~((uint8_t)abs(val) | 0x80);
        }
        else {
            mouse_axes[port][index] = ~((uint8_t)val);
        }
    }
}

static inline void load_trackball_axes(uint8_t port) {
    uint8_t *relative = (uint8_t *)(wired_adapter.data[port].output + 2);
    int32_t *raw_axes = (int32_t *)(wired_adapter.data[port].output + 4);
    int32_t val = 0;
    uint8_t shift[2] = {4, 0};

    mouse_axes[port][0] = 0;

    for (uint32_t i = 0; i < 2; i++) {
        if (relative[i]) {
            val = atomic_clear(&raw_axes[i]);
        }
        else {
            val = raw_axes[i];
        }

        if (val > 7) {
            mouse_axes[port][0] |= (7 & 0xF) << shift[i];
        }
        else if (val < -8) {
            mouse_axes[port][0] |= (((uint8_t)-8) & 0xF) << shift[i];
        }
        else {
            mouse_axes[port][0] |= ((uint8_t)val & 0xF) << shift[i];
        }
    }
}

static uint32_t npiso_isr(uint32_t cause) {
    const uint32_t low_io = GPIO.acpu_int;
    const uint32_t high_io = GPIO.acpu_int1.intr;

    /* reset bit counter, set first bit */
    if (high_io & NPISO_LATCH_MASK) {
        for (uint32_t i = 0; i < NPISO_PORT_MAX; i++) {
            switch (dev_type[i]) {
                case DEV_FC_TRACKBALL:
                    set_data(i, 1, wired_adapter.data[i].output[0] & 0x80);
                    break;
                case DEV_FC_MULTITAP_ALT:
                    set_data(i, 1, wired_adapter.data[i + 2].output[0] & 0x80);
                __attribute__ ((fallthrough));
                default:
                    set_data(i, 0, wired_adapter.data[i].output[0] & 0x80);
                    break;
            }
            cnt[i] = 1;
            mask[i] = 0x40;
        }
    }

    /* data idx */
    idx[0] = cnt[0] >> 3;
    idx[1] = cnt[1] >> 3;

    /* Update data lines on rising clock edge */
    for (uint32_t i = 0; i < NPISO_PORT_MAX; i++) {
        uint32_t clk_mask = gpio_mask[i][NPISO_CLK];
        if (low_io & clk_mask) {
            switch (dev_type[i]) {
                case DEV_FC_NES_PAD:
                    while (!(GPIO.in & clk_mask)); /* Wait rising edge */
                    if (idx[i]) {
                        set_data(i, 0, 0);
                    }
                    else {
                        set_data(i, 0, wired_adapter.data[i].output[0] & mask[i]);
                    }
                    break;
                case DEV_FC_TRACKBALL:
                    while (!(GPIO.in & clk_mask)); /* Wait rising edge */
                    switch (idx[i]) {
                        case 0:
                            set_data(i, 1, wired_adapter.data[i].output[0] & mask[i]);

                            /* Prepare YX axis for next byte on free time */
                            if (mask[i] == 0x01) {
                                load_trackball_axes(i);
                            }
                            break;
                        case 1:
                            set_data(i, 1, mouse_axes[i][0] & mask[i]);
                            break;
                        case 2:
                            set_data(i, 1, wired_adapter.data[i].output[1] & mask[i]);
                            break;
                        default:
                            set_data(i, 1, 0);
                            break;
                    }
                    break;
                case DEV_FC_NES_MULTITAP:
                    while (!(GPIO.in & clk_mask)); /* Wait rising edge */
                    switch (idx[i]) {
                        case 0:
                            set_data(i, 0, wired_adapter.data[i].output[0] & mask[i]);
                            break;
                        case 1:
                            set_data(i, 0, wired_adapter.data[i + 2].output[0] & mask[i]);
                            break;
                        case 2:
                            set_data(i, 0, fs_id[i] & mask[i]);
                            break;
                        default:
                            set_data(i, 0, 0);
                            break;
                    }
                    break;
                case DEV_FC_MULTITAP_ALT:
                    while (!(GPIO.in & clk_mask)); /* Wait rising edge */
                    if (idx[i]) {
                        set_data(i, 0, 0);
                        set_data(i, 1, 0);
                    }
                    else {
                        set_data(i, 0, wired_adapter.data[i].output[0] & mask[i]);
                        set_data(i, 1, wired_adapter.data[i + 2].output[0] & mask[i]);
                    }
                    break;
                case DEV_SFC_SNES_PAD:
                    while (!(GPIO.in & clk_mask)); /* Wait rising edge */
                    if (idx[i] > 1) {
                        set_data(i, 0, 0);
                    }
                    else {
                        set_data(i, 0, wired_adapter.data[i].output[idx[i]] & mask[i]);
                    }
                    break;
                case DEV_SFC_SNES_MOUSE:
                    if (GPIO.in1.val & NPISO_LATCH_MASK) {
                        mouse_update[i] = 1;
                    }
                    else {
                        while (!(GPIO.in & clk_mask)); /* Wait rising edge */
                        switch (idx[i]) {
                            case 0:
                                set_data(i, 0, wired_adapter.data[i].output[idx[i]] & mask[i]);

                                /* Prepare speed for next byte on free time */
                                if (mouse_update[i]) {
                                    mouse_speed[i] -= 0x10;
                                    if (mouse_speed[i] < MOUSE_SPEED_MAX) {
                                        mouse_speed[i] = MOUSE_SPEED_MIN;
                                    }
                                    mouse_update[i] = 0;
                                }
                                break;
                            case 1:
                                set_data(i, 0, (wired_adapter.data[i].output[idx[i]] & mask[i]) & mouse_speed[i]);

                                /* Prepare Y axis for next byte on free time */
                                if (mask[i] == 0x01) {
                                    load_mouse_axis(i, 0);
                                }
                                break;
                            case 2:
                                set_data(i, 0, mouse_axes[i][0] & mask[i]);

                                /* Prepare X axis for next byte on free time */
                                if (mask[i] == 0x01) {
                                    load_mouse_axis(i, 1);
                                }
                                break;
                            case 3:
                                set_data(i, 0, mouse_axes[i][1] & mask[i]);
                                break;
                            default:
                                set_data(i, 0, 0);
                                break;
                        }
                    }
                    break;
            }
            cnt[i]++;
            mask[i] >>= 1;
            if (!mask[i]) {
                mask[i] = 0x80;
            }
        }
    }

    if (high_io) GPIO.status1_w1tc.intr_st = high_io;
    if (low_io) GPIO.status_w1tc = low_io;

    return 0;
}

static uint32_t npiso_sfc_snes_5p_isr(uint32_t cause) {
    const uint32_t low_io = GPIO.acpu_int;
    const uint32_t high_io = GPIO.acpu_int1.intr;

    /* reset bit counter, set first bit */
    if (high_io & NPISO_LATCH_MASK) {
        if (GPIO.in1.val & NPISO_LATCH_MASK) {
            if (GPIO.in & P2_SEL_MASK) {
                //set_data(1, 0, 0);
                set_data(1, 1, 0);
            }
        }
        else {
            if (GPIO.in & P2_SEL_MASK) {
                set_data(1, 0, wired_adapter.data[1].output[0] & 0x80);
                set_data(1, 1, wired_adapter.data[2].output[0] & 0x80);
            }
            else {
                set_data(1, 0, wired_adapter.data[3].output[0] & 0x80);
                set_data(1, 1, wired_adapter.data[4].output[0] & 0x80);
            }
            cnts.val = 0x01010101;
            idx0 = 0;
            idx1 = 0;
            mask[0] = 0x40;
            mask[1] = 0x40;
        }
        if (!(GPIO.in1.val & NPISO_LATCH_MASK)) {
            /* Help for games with very short latch that don't trigger falling edge intr */
            if (GPIO.in & P2_SEL_MASK) {
                set_data(1, 0, wired_adapter.data[1].output[0] & 0x80);
                set_data(1, 1, wired_adapter.data[2].output[0] & 0x80);
            }
            else {
                set_data(1, 0, wired_adapter.data[3].output[0] & 0x80);
                set_data(1, 1, wired_adapter.data[4].output[0] & 0x80);
            }
            cnts.val = 0x01010101;
            idx0 = 0;
            idx1 = 0;
            mask[0] = 0x40;
            mask[1] = 0x40;
        }
        set_data(0, 0, wired_adapter.data[0].output[0] & 0x80);
    }

    if (low_io & P2_SEL_MASK) {
        if (GPIO.in & P2_SEL_MASK) {
            //set_data(1, 0, wired_adapter.data[1].output[0] & 0x80);
            //set_data(1, 1, wired_adapter.data[2].output[0] & 0x80);
            cnt0 = &cnts.cnt[0];
            cnt1 = &cnts.cnt[1];
        }
        else {
            set_data(1, 0, wired_adapter.data[3].output[0] & 0x80);
            set_data(1, 1, wired_adapter.data[4].output[0] & 0x80);
            cnt0 = &cnts.cnt[2];
            cnt1 = &cnts.cnt[3];
        }
        idx0 = *cnt0 >> 3;
        idx1 = *cnt1 >> 3;
        mask[0] = 0x40;
        mask[1] = 0x40;
    }

    /* Update port 0 */
    if (!(GPIO.in1.val & NPISO_LATCH_MASK)) {
        if (low_io & P1_CLK_MASK) {
            switch (idx0) {
                case 0:
                    while (!(GPIO.in & P1_CLK_MASK)); /* Wait rising edge */
                    set_data(0, 0, wired_adapter.data[0].output[0] & mask[0]);
                    break;
                case 1:
                    while (!(GPIO.in & P1_CLK_MASK)); /* Wait rising edge */
                    set_data(0, 0, wired_adapter.data[0].output[1] & mask[0]);
                    break;
                default:
                    while (!(GPIO.in & P1_CLK_MASK)); /* Wait rising edge */
                    set_data(0, 0, 0);
                    break;
            }
            (*cnt0)++;
            idx0 = *cnt0 >> 3;
            mask[0] >>= 1;
            if (!mask[0]) {
                mask[0] = 0x80;
            }
        }
    }

    /* Update port 1 */
    if (low_io & P2_CLK_MASK) {
        if (GPIO.in1.val & NPISO_LATCH_MASK) {
            /* P2-D0 load B when clocked while Latch is set. */
            while (!(GPIO.in & P2_CLK_MASK)); /* Wait rising edge */
            set_data(1, 0, wired_adapter.data[1].output[0] & 0x80);
        }
        else {
            if (GPIO.in & P2_SEL_MASK) {
                switch (idx1) {
                    case 0:
                        while (!(GPIO.in & P2_CLK_MASK)); /* Wait rising edge */
                        set_data(1, 0, wired_adapter.data[1].output[0] & mask[1]);
                        set_data(1, 1, wired_adapter.data[2].output[0] & mask[1]);
                        break;
                    case 1:
                        while (!(GPIO.in & P2_CLK_MASK)); /* Wait rising edge */
                        set_data(1, 0, wired_adapter.data[1].output[1] & mask[1]);
                        set_data(1, 1, wired_adapter.data[2].output[1] & mask[1]);
                        break;
                    default:
                        if (*cnt1 == 17) {
                            /* Hack to help for games reading too fast on SEL transition */
                            /* Set B following previous SEL controllers detection */
                            while (!(GPIO.in & P2_CLK_MASK)); /* Wait rising edge */
                            delay_us(2);
                            set_data(1, 0, wired_adapter.data[3].output[0] & 0x80);
                            set_data(1, 1, wired_adapter.data[4].output[0] & 0x80);
                        }
                        else {
                            while (!(GPIO.in & P2_CLK_MASK)); /* Wait rising edge */
                            set_data(1, 0, 0);
                            set_data(1, 1, 0);
                        }
                        break;
                }
            }
            else {
                switch (idx1) {
                    case 0:
                        while (!(GPIO.in & P2_CLK_MASK)); /* Wait rising edge */
                        set_data(1, 0, wired_adapter.data[3].output[0] & mask[1]);
                        set_data(1, 1, wired_adapter.data[4].output[0] & mask[1]);
                        break;
                    case 1:
                        while (!(GPIO.in & P2_CLK_MASK)); /* Wait rising edge */
                        set_data(1, 0, wired_adapter.data[3].output[1] & mask[1]);
                        set_data(1, 1, wired_adapter.data[4].output[1] & mask[1]);
                        break;
                    default:
                        //if (*cnt1 == 17) {
                            /* Hack to help for games reading too fast on SEL transition */
                            /* Set B following previous SEL controllers detection */
                        //    set_data(1, 0, wired_adapter.data[1].output[0] & 0x80);
                        //    set_data(1, 1, wired_adapter.data[2].output[0] & 0x80);
                        //}
                        //else {
                            while (!(GPIO.in & P2_CLK_MASK)); /* Wait rising edge */
                            set_data(1, 0, 0);
                            set_data(1, 1, 0);
                        //}
                        break;
                }
            }
            (*cnt1)++;
            idx1 = *cnt1 >> 3;
            mask[1] >>= 1;
            if (!mask[1]) {
                mask[1] = 0x80;
            }
        }
    }

    /* EA games Latch sometimes glitch and we can't detect 2nd rising */
    if (GPIO.in1.val & NPISO_LATCH_MASK) {
        if (GPIO.in & P2_SEL_MASK) {
            set_data(1, 1, 0);
        }
    }

    if (high_io) GPIO.status1_w1tc.intr_st = high_io;
    if (low_io) GPIO.status_w1tc = low_io;

    return 0;
}

void npiso_init(void)
{
    gpio_config_t io_conf = {0};

    if (wired_adapter.system_id == NES) {
        switch (config.global_cfg.multitap_cfg) {
            case MT_SLOT_1:
            case MT_SLOT_2:
            case MT_DUAL:
                dev_type[0] = DEV_FC_NES_MULTITAP;
                dev_type[1] = DEV_FC_NES_MULTITAP;
                mt_first_port[1] = 2;
                break;
            case MT_ALT:
                dev_type[0] = DEV_FC_MULTITAP_ALT;
                dev_type[1] = DEV_FC_MULTITAP_ALT;
                break;
            default:
                mt_first_port[1] = 1;
                break;
        }

        for (uint32_t i = 0; i < NPISO_PORT_MAX; i++) {
            if (dev_type[i] == DEV_NONE) {
                switch (config.out_cfg[i].dev_mode) {
                    case DEV_PAD:
                    case DEV_PAD_ALT:
                        dev_type[i] = DEV_FC_NES_PAD;
                        break;
                    case DEV_KB:
                        dev_type[i] = DEV_FC_KB;
                        break;
                    case DEV_MOUSE:
                        dev_type[i] = DEV_FC_TRACKBALL;
                        break;
                }
            }
        }
    }
    else {
        switch (config.global_cfg.multitap_cfg) {
            case MT_SLOT_1:
                dev_type[0] = DEV_SFC_SNES_MULTITAP;
                mt_first_port[1] = 4;
                break;
            case MT_SLOT_2:
                dev_type[1] = DEV_SFC_SNES_MULTITAP;
                mt_first_port[1] = 1;
                break;
            case MT_DUAL:
                dev_type[0] = DEV_SFC_SNES_MULTITAP;
                dev_type[1] = DEV_SFC_SNES_MULTITAP;
                mt_first_port[1] = 4;
                break;
            default:
                mt_first_port[1] = 1;
                break;
        }

        for (uint32_t i = 0; i < NPISO_PORT_MAX; i++) {
            if (dev_type[i] == DEV_NONE) {
                switch (config.out_cfg[i].dev_mode) {
                    case DEV_PAD:
                    case DEV_PAD_ALT:
                        dev_type[i] = DEV_SFC_SNES_PAD;
                        break;
                    case DEV_KB:
                        dev_type[i] = DEV_SNES_XBAND_KB;
                        break;
                    case DEV_MOUSE:
                        dev_type[i] = DEV_SFC_SNES_MOUSE;
                        break;
                }
            }
        }
    }

    /* Latch */
    if (dev_type[1] == DEV_SFC_SNES_MULTITAP) {
        io_conf.intr_type = GPIO_INTR_ANYEDGE;
    }
    else {
        io_conf.intr_type = GPIO_INTR_POSEDGE;
    }
    io_conf.pin_bit_mask = 1ULL << NPISO_LATCH_PIN;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config_iram(&io_conf);

    /* Clocks */
    for (uint32_t i = 0; i < NPISO_PORT_MAX; i++) {
        io_conf.intr_type = GPIO_INTR_NEGEDGE;
        io_conf.pin_bit_mask = 1ULL << gpio_pins[i][NPISO_CLK];
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config_iram(&io_conf);
    }

    /* D0, D1 */
    for (uint32_t i = 0; i < NPISO_PORT_MAX; i++) {
        for (uint32_t j = NPISO_D0; j <= NPISO_D1; j++) {
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.pin_bit_mask = 1ULL << gpio_pins[i][j];
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            gpio_config_iram(&io_conf);
            set_data(i, j &  0x1, 1);
        }
    }

    /* Consolize VBOY VTAP buttons */
    if (wired_adapter.system_id == VBOY) {
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.pin_bit_mask = 1ULL << VTAP_PAL_PIN;
        io_conf.mode = GPIO_MODE_OUTPUT_OD;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config_iram(&io_conf);
        gpio_set_level_iram(VTAP_PAL_PIN, 1);
        io_conf.pin_bit_mask = 1ULL << VTAP_MODE_PIN;
        gpio_config_iram(&io_conf);
        gpio_set_level_iram(VTAP_MODE_PIN, 1);
    }

    if (dev_type[1] == DEV_SFC_SNES_MULTITAP) {
        /* Selects */
        for (uint32_t i = 0; i < NPISO_PORT_MAX; i++) {
            io_conf.intr_type = GPIO_INTR_ANYEDGE;
            io_conf.pin_bit_mask = 1ULL << gpio_pins[i][NPISO_SEL];
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            gpio_config_iram(&io_conf);
        }

        intexc_alloc_iram(ETS_GPIO_INTR_SOURCE, 19, npiso_sfc_snes_5p_isr);
    }
    else {
        intexc_alloc_iram(ETS_GPIO_INTR_SOURCE, 19, npiso_isr);
    }
}
