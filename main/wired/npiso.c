#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <xtensa/hal.h>
#include <esp_intr_alloc.h>
#include "driver/gpio.h"
#include "../zephyr/types.h"
#include "../util.h"
#include "../adapter/adapter.h"
#include "../adapter/config.h"
#include "npiso.h"

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

#if 0
static const uint32_t gpio_mask[NPISO_PORT_MAX][NPISO_PIN_MAX] = {
    {P1_CLK_MASK, P1_SEL_MASK, P1_D0_MASK, P1_D1_MASK},
    {P2_CLK_MASK, P2_SEL_MASK, P2_D0_MASK, P2_D1_MASK},
};
#endif

static uint8_t dev_type[NPISO_PORT_MAX] = {0};
static uint8_t mt_first_port[NPISO_PORT_MAX] = {0};
static uint32_t cnt[2] = {0};
static uint8_t mask[2] = {0x80, 0x80};
static struct counters cnts = {0};
static uint8_t *cnt0 = &cnts.cnt[0];
static uint8_t *cnt1 = &cnts.cnt[1];
static uint32_t idx0, idx1;

static void IRAM_ATTR set_data(uint8_t port, uint8_t data_id, uint8_t value) {
    uint8_t pin = gpio_pins[port][NPISO_D0 + data_id];

    if (value) {
        GPIO.out_w1ts = BIT(pin);
    }
    else {
        GPIO.out_w1tc = BIT(pin);
    }
}

static void IRAM_ATTR npiso_fc_nes_2p_isr(void* arg) {
    const uint32_t low_io = GPIO.acpu_int;
    const uint32_t high_io = GPIO.acpu_int1.intr;

    /* reset bit counter, set first bit */
    if (high_io & NPISO_LATCH_MASK) {
        set_data(0, 0, wired_adapter.data[0].output[0] & 0x80);
        set_data(1, 0, wired_adapter.data[1].output[0] & 0x80);
        cnt[0] = 1;
        cnt[1] = 1;
        mask[0] = 0x40;
        mask[1] = 0x40;
    }

    /* Update port 0 */
    if (low_io & P1_CLK_MASK) {
        while (!(GPIO.in & P1_CLK_MASK)); /* Wait rising edge */
        if (cnt[0] > 7) {
            set_data(0, 0, 0);
        }
        else {
            set_data(0, 0, wired_adapter.data[0].output[0] & mask[0]);
        }
        cnt[0]++;
        mask[0] >>= 1;
    }

    /* Update port 1 */
    if (low_io & P2_CLK_MASK) {
        while (!(GPIO.in & P2_CLK_MASK)); /* Wait rising edge */
        if (cnt[1] > 7) {
            set_data(1, 0, 0);
        }
        else {
            set_data(1, 0, wired_adapter.data[1].output[0] & mask[1]);
        }
        cnt[1]++;
        mask[1] >>= 1;
    }

    if (high_io) GPIO.status1_w1tc.intr_st = high_io;
    if (low_io) GPIO.status_w1tc = low_io;
}

static void IRAM_ATTR npiso_fc_4p_isr(void* arg) {
    const uint32_t low_io = GPIO.acpu_int;
    const uint32_t high_io = GPIO.acpu_int1.intr;

    /* reset bit counter, set first bit */
    if (high_io & NPISO_LATCH_MASK) {
        set_data(0, 0, wired_adapter.data[0].output[0] & 0x80);
        set_data(1, 0, wired_adapter.data[1].output[0] & 0x80);
        set_data(0, 1, wired_adapter.data[2].output[0] & 0x80);
        set_data(1, 1, wired_adapter.data[3].output[0] & 0x80);
        cnt[0] = 1;
        cnt[1] = 1;
        mask[0] = 0x40;
        mask[1] = 0x40;
    }

    /* Update port 0 */
    if (low_io & P1_CLK_MASK) {
        while (!(GPIO.in & P1_CLK_MASK)); /* Wait rising edge */
        if (cnt[0] > 7) {
            set_data(0, 0, 0);
            set_data(0, 1, 0);
        }
        else {
            set_data(0, 0, wired_adapter.data[0].output[0] & mask[0]);
            set_data(0, 1, wired_adapter.data[2].output[0] & mask[0]);
        }
        cnt[0]++;
        mask[0] >>= 1;
    }

    /* Update port 1 */
    if (low_io & P2_CLK_MASK) {
        while (!(GPIO.in & P2_CLK_MASK)); /* Wait rising edge */
        if (cnt[1] > 7) {
            set_data(1, 0, 0);
            set_data(1, 1, 0);
        }
        else {
            set_data(1, 0, wired_adapter.data[1].output[0] & mask[1]);
            set_data(1, 1, wired_adapter.data[3].output[0] & mask[1]);
        }
        cnt[1]++;
        mask[1] >>= 1;
    }

    if (high_io) GPIO.status1_w1tc.intr_st = high_io;
    if (low_io) GPIO.status_w1tc = low_io;
}

static void IRAM_ATTR npiso_nes_fs_isr(void* arg) {
    const uint32_t low_io = GPIO.acpu_int;
    const uint32_t high_io = GPIO.acpu_int1.intr;

    /* reset bit counter, set first bit */
    if (high_io & NPISO_LATCH_MASK) {
        set_data(0, 0, wired_adapter.data[0].output[0] & 0x80);
        set_data(1, 0, wired_adapter.data[1].output[0] & 0x80);
        cnt[0] = 1;
        cnt[1] = 1;
        mask[0] = 0x40;
        mask[1] = 0x40;
    }

    idx0 = cnt[0] >> 3;
    idx1 = cnt[1] >> 3;

    /* Update port 0 */
    if (low_io & P1_CLK_MASK) {
        while (!(GPIO.in & P1_CLK_MASK)); /* Wait rising edge */
        switch (idx0) {
            case 0:
                set_data(0, 0, wired_adapter.data[0].output[0] & mask[0]);
                break;
            case 1:
                set_data(0, 0, wired_adapter.data[2].output[0] & mask[0]);
                break;
            case 2:
                set_data(0, 0, 0xEF & mask[0]);
                break;
            default:
                set_data(0, 0, 0);
                break;
        }
        cnt[0]++;
        mask[0] >>= 1;
        if (!mask[0]) {
            mask[0] = 0x80;
        }
    }

    /* Update port 1 */
    if (low_io & P2_CLK_MASK) {
        while (!(GPIO.in & P2_CLK_MASK)); /* Wait rising edge */
        switch (idx1) {
            case 0:
                set_data(1, 0, wired_adapter.data[1].output[0] & mask[1]);
                break;
            case 1:
                set_data(1, 0, wired_adapter.data[3].output[0] & mask[1]);
                break;
            case 2:
                set_data(1, 0, 0xDF & mask[1]);
                break;
            default:
                set_data(1, 0, 0);
                break;
        }
        cnt[1]++;
        mask[1] >>= 1;
        if (!mask[1]) {
            mask[1] = 0x80;
        }
    }

    if (high_io) GPIO.status1_w1tc.intr_st = high_io;
    if (low_io) GPIO.status_w1tc = low_io;
}

static void IRAM_ATTR npiso_sfc_snes_2p_isr(void* arg) {
    const uint32_t low_io = GPIO.acpu_int;
    const uint32_t high_io = GPIO.acpu_int1.intr;

    /* reset bit counter, set first bit */
    if (high_io & NPISO_LATCH_MASK) {
        set_data(0, 0, wired_adapter.data[0].output[0] & 0x80);
        set_data(1, 0, wired_adapter.data[1].output[0] & 0x80);
        cnt[0] = 1;
        cnt[1] = 1;
        mask[0] = 0x40;
        mask[1] = 0x40;
    }

    idx0 = cnt[0] >> 3;
    idx1 = cnt[1] >> 3;

    /* Update port 0 */
    if (low_io & P1_CLK_MASK) {
        while (!(GPIO.in & P1_CLK_MASK)); /* Wait rising edge */
        switch (idx0) {
            case 0:
                set_data(0, 0, wired_adapter.data[0].output[0] & mask[0]);
                break;
            case 1:
                set_data(0, 0, wired_adapter.data[0].output[1] & mask[0]);
                break;
            default:
                set_data(0, 0, 0);
                break;
        }
        cnt[0]++;
        mask[0] >>= 1;
        if (!mask[0]) {
            mask[0] = 0x80;
        }
    }

    /* Update port 1 */
    if (low_io & P2_CLK_MASK) {
        while (!(GPIO.in & P2_CLK_MASK)); /* Wait rising edge */
        switch (idx1) {
            case 0:
                set_data(1, 0, wired_adapter.data[1].output[0] & mask[1]);
                break;
            case 1:
                set_data(1, 0, wired_adapter.data[1].output[1] & mask[1]);
                break;
            default:
                set_data(1, 0, 0);
                break;
        }
        cnt[1]++;
        mask[1] >>= 1;
        if (!mask[1]) {
            mask[1] = 0x80;
        }
    }

    if (high_io) GPIO.status1_w1tc.intr_st = high_io;
    if (low_io) GPIO.status_w1tc = low_io;
}

static void IRAM_ATTR npiso_sfc_snes_5p_isr(void* arg) {
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
                            ets_delay_us(2);
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
    if (wired_adapter.system_id == SNES) {
        io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
    }
    else {
        io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    }
    io_conf.pin_bit_mask = 1ULL << NPISO_LATCH_PIN;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    /* Clocks */
    for (uint32_t i = 0; i < NPISO_PORT_MAX; i++) {
        io_conf.intr_type = GPIO_PIN_INTR_NEGEDGE;
        io_conf.pin_bit_mask = 1ULL << gpio_pins[i][NPISO_CLK];
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    }

    /* Selects */
    for (uint32_t i = 0; i < NPISO_PORT_MAX; i++) {
        io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
        io_conf.pin_bit_mask = 1ULL << gpio_pins[i][NPISO_SEL];
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    }

    /* D0, D1 */
    for (uint32_t i = 0; i < NPISO_PORT_MAX; i++) {
        for (uint32_t j = NPISO_D0; j <= NPISO_D1; j++) {
            io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
            io_conf.pin_bit_mask = 1ULL << gpio_pins[i][j];
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            gpio_config(&io_conf);
            set_data(i, j &  0x1, 1);
        }
    }

    if (dev_type[0] == DEV_FC_NES_MULTITAP) {
        esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3, npiso_nes_fs_isr, NULL, NULL);
    }
    else if (dev_type[0] == DEV_FC_MULTITAP_ALT) {
        esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3, npiso_fc_4p_isr, NULL, NULL);
    }
    else if (dev_type[0] == DEV_SFC_SNES_PAD && dev_type[1] == DEV_SFC_SNES_MULTITAP) {
        esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3, npiso_sfc_snes_5p_isr, NULL, NULL);
    }
    else {
        if (wired_adapter.system_id == NES) {
            esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3, npiso_fc_nes_2p_isr, NULL, NULL);
        }
        else {
            esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3, npiso_sfc_snes_2p_isr, NULL, NULL);
        }
    }
}
