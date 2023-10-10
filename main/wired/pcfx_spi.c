/*
 * Copyright (c) 2019-2023, Jacques Gagnon
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
#include "adapter/wired/pcfx.h"
#include "wired_bare.h"
#include "pcfx_spi.h"

#define GPIO_INTR_NUM 19

#define P1_LATCH_PIN 33
#define P1_CLK_PIN 5
#define P1_DATA_PIN 19
#define P2_LATCH_PIN 26
#define P2_CLK_PIN 18
#define P2_DATA_PIN 22

#define P1_LATCH_MASK (1 << (P1_LATCH_PIN - 32))
#define P1_CLK_MASK (1 << P1_CLK_PIN)
#define P1_DATA_MASK (1 << P1_DATA_PIN)
#define P2_LATCH_MASK (1 << P2_LATCH_PIN)
#define P2_CLK_MASK (1 << P2_CLK_PIN)
#define P2_DATA_MASK (1 << P2_DATA_PIN)

#define PCFX_PORT_MAX 2

struct pcfx_ctrl_port {
    struct spi_cfg cfg;
    spi_dev_t *hw;
    uint32_t latch_pin;
    uint32_t clk_pin;
    uint32_t data_pin;
    uint32_t latch_mask;
    uint32_t clk_mask;
    uint32_t data_mask;
    uint8_t latch_sig;
    uint8_t clk_sig;
    uint8_t data_sig;
    uint8_t spi_mod;
};

static struct pcfx_ctrl_port pcfx_ctrl_ports[PCFX_PORT_MAX] = {
    {
        .cfg = {
            .hw = &SPI2,
            .write_bit_order = 1,
            .read_bit_order = 1,
            .clk_idle_edge = 1,
            .clk_i_edge = 0,
            .miso_delay_mode = 0,
            .miso_delay_num = 2,
            .mosi_delay_mode = 0,
            .mosi_delay_num = 3,
            .write_bit_len = 0,
            .read_bit_len = 33 - 1, // Extra bit to remove small gitch on packet end
            .inten = 0,
        },
        .hw = &SPI2,
        .latch_pin = P1_LATCH_PIN,
        .clk_pin = P1_CLK_PIN,
        .data_pin = P1_DATA_PIN,
        .latch_mask = P1_LATCH_MASK,
        .clk_mask = P1_CLK_MASK,
        .data_mask = P1_DATA_MASK,
        .latch_sig = HSPICS0_IN_IDX,
        .clk_sig = HSPICLK_IN_IDX,
        .data_sig = HSPIQ_OUT_IDX,
        .spi_mod = PERIPH_HSPI_MODULE,
    },
    {
        .cfg = {
            .hw = &SPI3,
            .write_bit_order = 1,
            .read_bit_order = 1,
            .clk_idle_edge = 1,
            .clk_i_edge = 0,
            .miso_delay_mode = 0,
            .miso_delay_num = 2,
            .mosi_delay_mode = 0,
            .mosi_delay_num = 3,
            .write_bit_len = 0,
            .read_bit_len = 33 - 1, // Extra bit to remove small gitch on packet end
            .inten = 0,
        },
        .hw = &SPI3,
        .latch_pin = P2_LATCH_PIN,
        .clk_pin = P2_CLK_PIN,
        .data_pin = P2_DATA_PIN,
        .latch_mask = P2_LATCH_MASK,
        .clk_mask = P2_CLK_MASK,
        .data_mask = P2_DATA_MASK,
        .latch_sig = VSPICS0_IN_IDX,
        .clk_sig = VSPICLK_IN_IDX,
        .data_sig = VSPIQ_OUT_IDX,
        .spi_mod = PERIPH_VSPI_MODULE,
    },
};

static inline void load_mouse_axes(uint8_t port, uint8_t *axes) {
    uint8_t *relative = (uint8_t *)(wired_adapter.data[port].output + 6);
    int32_t *raw_axes = (int32_t *)(wired_adapter.data[port].output + 8);
    int32_t val = 0;

    for (uint32_t i = 0; i < 2; i++) {
        if (relative[i]) {
            val = atomic_clear(&raw_axes[i]);
        }
        else {
            val = raw_axes[i];
        }

        if (val > 127) {
            axes[i] = 127;
        }
        else if (val < -128) {
            axes[i] = -128;
        }
        else {
            axes[i] = (uint8_t)val;
        }
    }
}

static inline void load_buffer(uint8_t port) {
    switch (config.out_cfg[port].dev_mode) {
        case DEV_PAD:
        {
            uint32_t tmp = wired_adapter.data[port].output32[0] & wired_adapter.data[port].output_mask32[0];
            pcfx_ctrl_ports[port].hw->data_buf[0] = tmp;
            break;
        }
        case DEV_MOUSE:
        {
            uint8_t tmp[4];
            load_mouse_axes(port, tmp);
            memcpy(&tmp[2], &wired_adapter.data[port].output[2], 2);
            pcfx_ctrl_ports[port].hw->data_buf[0] = *(uint32_t *)tmp;
            break;
        }
    }
}

static unsigned latch_isr(unsigned cause) {
    const uint32_t low_io = GPIO.acpu_int;
    const uint32_t high_io = GPIO.acpu_int1.intr;
    uint8_t port = 0;
    struct pcfx_ctrl_port *p;

    if (high_io & P1_LATCH_MASK) {
        port = 0;
    }
    else if (low_io & P2_LATCH_MASK) {
        port = 1;
    }
    else {
        goto exit;
    }

    p = &pcfx_ctrl_ports[port];

    p->hw->data_buf[1] = 0xFFFFFFFF;
    load_buffer(port);
    p->hw->slave.sync_reset = 1;
    p->hw->slave.trans_done = 0;
    p->hw->cmd.usr = 1;

    ++wired_adapter.data[port].frame_cnt;
    pcfx_gen_turbo_mask(&wired_adapter.data[port]);

exit:
    if (high_io) GPIO.status1_w1tc.intr_st = high_io;
    if (low_io) GPIO.status_w1tc = low_io;
    return 0;
}

void pcfx_spi_init(uint32_t package) {
    gpio_config_t io_conf = {0};

    for (uint32_t i = 0; i < PCFX_PORT_MAX; i++) {
        struct pcfx_ctrl_port *p = &pcfx_ctrl_ports[i];

        /* Latch */
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.intr_type = GPIO_PIN_INTR_NEGEDGE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pin_bit_mask = 1ULL << p->latch_pin;
        gpio_config_iram(&io_conf);
        gpio_matrix_in(p->latch_pin, p->latch_sig, true); // Invert latch to use as CS

        /* Data */
        gpio_set_level_iram(p->data_pin, 1);
        gpio_set_direction_iram(p->data_pin, GPIO_MODE_OUTPUT);
        gpio_matrix_out(p->data_pin, p->data_sig, true, false); // PCFX data is inverted
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[p->data_pin], PIN_FUNC_GPIO);

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
