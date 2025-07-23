/*
 * Copyright (c) 2019-2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "soc/io_mux_reg.h"
#include "esp_private/periph_ctrl.h"
#include <soc/spi_periph.h>
#include <esp32/rom/ets_sys.h>
#include <esp32/rom/gpio.h>
#include "hal/clk_gate_ll.h"
#include "driver/gpio.h"
#include "system/intr.h"
#include "system/gpio.h"
#include "system/delay.h"
#include "zephyr/atomic.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "adapter/wired/npiso.h"
#include "wired_bare.h"
#include "snes_spi.h"

#define GPIO_INTR_NUM 19

#define P1_LATCH_PIN 32
#define P1_CLK_PIN 5
#define P1_CIPO_PIN 19
#define P1_COPI_PIN 23
#define P2_LATCH_PIN 32
#define P2_CLK_PIN 18
#define P2_CIPO_PIN 22
#define P2_COPI_PIN 26

#define P1_LATCH_MASK (1 << (P1_LATCH_PIN - 32))
#define P1_CLK_MASK (1 << P1_CLK_PIN)
#define P1_CIPO_MASK (1 << P1_CIPO_PIN)
#define P1_COPI_MASK (1 << P1_COPI_PIN)
#define P2_LATCH_MASK (1 << (P2_LATCH_PIN - 32))
#define P2_CLK_MASK (1 << P2_CLK_PIN)
#define P2_CIPO_MASK (1 << P2_CIPO_PIN)
#define P2_COPI_MASK (1 << P2_COPI_PIN)

#define SNES_PORT_MAX 2

enum {
    SNES_PAD_FORMAT_DEFAULT = 0,
    SNES_PAD_FORMAT_BR_8BITS,
    SNES_PAD_FORMAT_BR_4BITS,
};

struct snes_ctrl_port {
    struct spi_cfg cfg;
    spi_dev_t *hw;
    uint32_t latch_pin;
    uint32_t clk_pin;
    uint32_t cipo_pin;
    uint32_t copi_pin;
    uint32_t latch_mask;
    uint32_t clk_mask;
    uint32_t cipo_mask;
    uint32_t copi_mask;
    uint8_t latch_sig;
    uint8_t clk_sig;
    uint8_t cipo_sig;
    uint8_t copi_sig;
    uint8_t spi_mod;
    uint8_t rumble_data;
    uint8_t format;
};

static struct snes_ctrl_port snes_ctrl_ports[SNES_PORT_MAX] = {
    {
        .cfg = {
            .hw = &SPI2,
            .write_bit_order = 0,
            .read_bit_order = 0,
            .clk_idle_edge = 1,
            .clk_i_edge = 0,
            .miso_delay_mode = 0,
            .miso_delay_num = 2,
            .mosi_delay_mode = 0,
            .mosi_delay_num = 3,
            .write_bit_len = 64 - 1,
            .read_bit_len = 64 - 1, // Extra bit to remove small gitch on packet end
            .inten = 0,
        },
        .hw = &SPI2,
        .latch_pin = P1_LATCH_PIN,
        .clk_pin = P1_CLK_PIN,
        .cipo_pin = P1_CIPO_PIN,
        .copi_pin = P1_COPI_PIN,
        .latch_mask = P1_LATCH_MASK,
        .clk_mask = P1_CLK_MASK,
        .cipo_mask = P1_CIPO_MASK,
        .copi_mask = P1_COPI_MASK,
        .latch_sig = HSPICS0_IN_IDX,
        .clk_sig = HSPICLK_IN_IDX,
        .cipo_sig = HSPIQ_OUT_IDX,
        .copi_sig = HSPID_IN_IDX,
        .spi_mod = PERIPH_HSPI_MODULE,
    },
    {
        .cfg = {
            .hw = &SPI3,
            .write_bit_order = 0,
            .read_bit_order = 0,
            .clk_idle_edge = 1,
            .clk_i_edge = 0,
            .miso_delay_mode = 0,
            .miso_delay_num = 2,
            .mosi_delay_mode = 0,
            .mosi_delay_num = 3,
            .write_bit_len = 64 - 1,
            .read_bit_len = 64 - 1, // Extra bit to remove small gitch on packet end
            .inten = 0,
        },
        .hw = &SPI3,
        .latch_pin = P2_LATCH_PIN,
        .clk_pin = P2_CLK_PIN,
        .cipo_pin = P2_CIPO_PIN,
        .copi_pin = P2_COPI_PIN,
        .latch_mask = P2_LATCH_MASK,
        .clk_mask = P2_CLK_MASK,
        .cipo_mask = P2_CIPO_MASK,
        .copi_mask = P2_COPI_MASK,
        .latch_sig = VSPICS0_IN_IDX,
        .clk_sig = VSPICLK_IN_IDX,
        .cipo_sig = VSPIQ_OUT_IDX,
        .copi_sig = VSPID_IN_IDX,
        .spi_mod = PERIPH_VSPI_MODULE,
    },
};

static inline void load_mouse_axes(uint8_t port, uint8_t *axes) {
    uint8_t *relative = (uint8_t *)(wired_adapter.data[port].output + 2);
    int32_t *raw_axes = (int32_t *)(wired_adapter.data[port].output + 4);
    int32_t val = 0;

    for (uint32_t i = 0; i < 2; i++) {
        if (relative[i]) {
            val = atomic_clear(&raw_axes[i]);
        }
        else {
            val = raw_axes[i];
        }

        if (val > 127) {
            axes[i] = 128;
        }
        else if (val < -128) {
            axes[i] = 0;
        }
        else {
            if (val < 0) {
                axes[i] = ~((uint8_t)abs(val) | 0x80);
            }
            else {
                axes[i] = ~((uint8_t)val);
            }
        }
    }
}

static inline void load_buffer(uint8_t port) {
    switch (config.out_cfg[port].dev_mode) {
        case DEV_PAD:
        {
            union {
                uint8_t tmp[8];
                uint16_t tmp16[4];
                uint32_t tmp32[2];
            } raw = {0};

            switch (snes_ctrl_ports[port].format) {
                case SNES_PAD_FORMAT_BR_8BITS:
                    raw.tmp16[0] = wired_adapter.data[port].output16[0] | wired_adapter.data[port].output_mask16[0];
                    raw.tmp16[1] = wired_adapter.data[port].output16[1];
                    raw.tmp32[1] = wired_adapter.data[port].output32[1];
                    raw.tmp[1] &= 0xF0;
                    raw.tmp[1] |= 0x09;
                    break;
                case SNES_PAD_FORMAT_BR_4BITS:
                    raw.tmp16[0] = wired_adapter.data[port].output16[0] | wired_adapter.data[port].output_mask16[0];
                    raw.tmp[2] = (wired_adapter.data[port].output[2] & 0xF0) | (wired_adapter.data[port].output[3] >> 4);
                    raw.tmp[3] = (wired_adapter.data[port].output[4] & 0xF0) | (wired_adapter.data[port].output[5] >> 4);
                    raw.tmp[4] = (wired_adapter.data[port].output[6] & 0xF0) | (wired_adapter.data[port].output[7] >> 4);
                    raw.tmp[1] &= 0xF0;
                    raw.tmp[1] |= 0x08;
                    break;
                default:
                    raw.tmp16[0] = wired_adapter.data[port].output16[0] | wired_adapter.data[port].output_mask16[0];
                    raw.tmp16[1] = 0;
                    raw.tmp32[1] = 0;
                    raw.tmp[1] |= 0x0F;
                    if ((uint8_t)~wired_adapter.data[port].output[6] > 16) {
                        raw.tmp16[0] &= ~(1 << 13);
                    }
                    if ((uint8_t)~wired_adapter.data[port].output[7] > 16) {
                        raw.tmp16[0] &= ~(1 << 12);
                    }
                    break;
            }

            snes_ctrl_ports[port].hw->data_buf[0] = raw.tmp32[0];
            snes_ctrl_ports[port].hw->data_buf[1] = raw.tmp32[1];
            break;
        }
        case DEV_MOUSE:
        {
            union {
                uint8_t tmp[4];
                uint32_t tmp32;
            } raw = {0};
            raw.tmp[0] = 0xFF;
            raw.tmp[1] = wired_adapter.data[port].output[1];
            load_mouse_axes(port, &raw.tmp[2]);
            snes_ctrl_ports[port].hw->data_buf[0] = raw.tmp32;
            break;
        }
    }
}

static unsigned latch_isr(unsigned cause) {
    const uint32_t low_io = GPIO.acpu_int;
    const uint32_t high_io = GPIO.acpu_int1.intr;
    struct snes_ctrl_port *p;

    if (high_io & P1_LATCH_MASK) {
        for (uint32_t port = 0; port < SNES_PORT_MAX; port++) {
            union {
                uint32_t tmp32[2];
                uint8_t tmp[8];
            } raw;

            raw.tmp32[0] = snes_ctrl_ports[port].hw->data_buf[0];
            raw.tmp32[1] = snes_ctrl_ports[port].hw->data_buf[1];
            p = &snes_ctrl_ports[port];

            load_buffer(port);
            p->hw->slave.sync_reset = 1;
            p->hw->slave.trans_done = 0;
            p->hw->cmd.usr = 1;

            ++wired_adapter.data[port].frame_cnt;
            npiso_gen_turbo_mask(&wired_adapter.data[port]);

            uint8_t cmd_sentry = (raw.tmp[2] << 1) | (raw.tmp[3] >> 7);
            uint8_t cmd_data = (raw.tmp[3] << 1) | (raw.tmp[4] >> 7);
            if (cmd_sentry != 'r' && cmd_sentry != 'b') {
                cmd_sentry = raw.tmp[2];
                cmd_data = raw.tmp[3];
            }

            switch (cmd_sentry) {
                case 'b':
                    p->format = cmd_data;
                    break;
                case 'r':
                    if (cmd_data != p->rumble_data
                            && config.out_cfg[port].acc_mode & ACC_RUMBLE) {
                        struct raw_fb fb_data = {0};
                        fb_data.data[0] = (cmd_data & 0xF0) >> 4;
                        fb_data.data[1] = cmd_data & 0x0F;
                        fb_data.header.wired_id = port;
                        fb_data.header.type = FB_TYPE_RUMBLE;
                        fb_data.header.data_len = 2;
                        adapter_q_fb(&fb_data);
                    }
                    p->rumble_data = cmd_data;
                    //ets_printf("%02X %02X\n", cmd_sentry, cmd_data);
                    break;
            }
        }
    }

    if (high_io) GPIO.status1_w1tc.intr_st = high_io;
    if (low_io) GPIO.status_w1tc = low_io;
    return 0;
}

void snes_spi_init(uint32_t package) {
    gpio_config_t io_conf = {0};

    for (uint32_t i = 0; i < SNES_PORT_MAX; i++) {
        struct snes_ctrl_port *p = &snes_ctrl_ports[i];

        /* Latch */
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pin_bit_mask = 1ULL << p->latch_pin;
        gpio_config_iram(&io_conf);
        gpio_matrix_in(p->latch_pin, p->latch_sig, false);

        /* CIPO */
        gpio_set_level_iram(p->cipo_pin, 1);
        gpio_set_direction_iram(p->cipo_pin, GPIO_MODE_OUTPUT);
        gpio_matrix_out(p->cipo_pin, p->cipo_sig, false, false);
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[p->cipo_pin], PIN_FUNC_GPIO);

        /* COPI */
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pin_bit_mask = 1ULL << p->copi_pin;
        gpio_config_iram(&io_conf);
        gpio_matrix_in(p->copi_pin, p->copi_sig, false);

        /* Clock */
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pin_bit_mask = 1ULL << p->clk_pin;
        gpio_config_iram(&io_conf);
        gpio_matrix_in(p->clk_pin, p->clk_sig, true);

        periph_ll_enable_clk_clear_rst(p->spi_mod);

        spi_init(&p->cfg);
    }

    intexc_alloc_iram(ETS_GPIO_INTR_SOURCE, GPIO_INTR_NUM, latch_isr);
}
