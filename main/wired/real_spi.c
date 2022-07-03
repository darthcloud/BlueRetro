/*
 * Copyright (c) 2019-2022, Jacques Gagnon
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
#include "adapter/wired/real.h"
#include "real_spi.h"

enum {
    DEV_NONE = 0,
    DEV_3DO_PAD,
    DEV_3DO_MOUSE,
    DEV_TYPE_MAX,
};

#define SPI2_HSPI_INTR_NUM 19
#define SPI3_VSPI_INTR_NUM 20
#define GPIO_INTR_NUM 21

#define SPI2_HSPI_INTR_MASK (1 << SPI2_HSPI_INTR_NUM)
#define SPI3_VSPI_INTR_MASK (1 << SPI3_VSPI_INTR_NUM)
#define GPIO_INTR_MASK (1 << GPIO_INTR_NUM)

/* Name relative to console */
#define P1_CS_OUT_PIN 19
#define P1_CS_IN_PIN 18
#define P1_SCK_PIN 22
#define P1_TXD_PIN 23
#define P1_RXD_PIN 21

#define P1_CS_OUT_MASK (1 << P1_CS_OUT_PIN)
#define P1_CS_IN_MASK (1 << P1_CS_IN_PIN)
#define P1_SCK_MASK (1 << P1_SCK_PIN)

#define SPI_LL_RST_MASK (SPI_OUT_RST | SPI_IN_RST | SPI_AHBM_RST | SPI_AHBM_FIFO_RST)
#define SPI_LL_UNUSED_INT_MASK  (SPI_INT_EN | SPI_SLV_WR_STA_DONE | SPI_SLV_RD_STA_DONE | SPI_SLV_WR_BUF_DONE | SPI_SLV_RD_BUF_DONE)

#define REAL_MAX_DEVICE 8
#define CLK_IDLE_TIMEOUT 1000

static uint32_t cs_state = 1;
static uint32_t idx = 0;
static uint8_t buffer[72];

static inline void load_mouse_axes(uint8_t port, uint8_t *axes) {
    uint8_t *relative = (uint8_t *)(wired_adapter.data[port].output + 2);
    int32_t *raw_axes = (int32_t *)(wired_adapter.data[port].output + 4);
    int32_t val = 0;
    uint16_t tmp = 0;

    for (uint32_t i = 0; i < 2; i++) {
        if (relative[i]) {
            val = atomic_clear(&raw_axes[i]);
        }
        else {
            val = raw_axes[i];
        }

        if (val > 511) {
            tmp = 511;
        }
        else if (val < -512) {
            tmp = -512;
        }
        else {
            tmp = (uint16_t)val;
        }

        switch (i) {
            case 0:
                axes[0] &= ~0x0F;
                axes[0] |= ((tmp & 0x3C0) >> 6);
                axes[1] &= ~0xFC;
                axes[1] |= ((tmp & 0x03F) << 2);
                break;
            case 1:
                axes[1] &= ~0x03;
                axes[1] |= ((tmp & 0x300) >> 8);
                axes[2] = (tmp & 0xFF);
                break;
        }
    }
}

static inline void write_buffer(const uint8_t *data, uint32_t len)
{
    for (int i = 0; i < len; i += 4) {
        //Use memcpy to get around alignment issues for txdata
        uint32_t word;
        memcpy(&word, &data[i], 4);
        SPI2.data_buf[(i / 4)] = word;
    }
}

static void load_buffer(void) {
    uint32_t size = 0;
    uint8_t *data = buffer;

    for (uint32_t i = 0; i < REAL_MAX_DEVICE; i++) {
        switch (config.out_cfg[i].dev_mode) {
            case DEV_PAD:
                *(uint16_t *)data = wired_adapter.data[i].output16[0] & wired_adapter.data[i].output_mask16[0];
                data += 2;
                size += 2;
                ++wired_adapter.data[i].frame_cnt;
                real_gen_turbo_mask(DEV_PAD, &wired_adapter.data[i]);
                break;
            case DEV_PAD_ALT:
                memcpy(data, wired_adapter.data[i].output, 7);
                *(uint16_t *)&data[7] = *(uint16_t *)&wired_adapter.data[i].output[7] & *(uint16_t *)&wired_adapter.data[i].output_mask[7];
                data += 9;
                size += 9;
                ++wired_adapter.data[i].frame_cnt;
                real_gen_turbo_mask(DEV_PAD_ALT, &wired_adapter.data[i]);
                break;
            case DEV_MOUSE:
                memcpy(data, wired_adapter.data[i].output, 2);
                load_mouse_axes(i, data + 1);
                data += 4;
                size += 4;
                break;
        }
    }
    write_buffer(buffer, size);
}

static void cs_generator(void) {
    uint32_t timeout, cur_in, prev_in, change;

    while (1) {
        timeout = 0;
        cur_in = prev_in = GPIO.in;
        while (!(change = cur_in ^ prev_in)) {
            prev_in = cur_in;
            cur_in = GPIO.in;
        }

        if (change & P1_SCK_MASK) {
            if (cur_in & P1_SCK_MASK) {
                while (GPIO.in & P1_SCK_MASK) {
                    if (++timeout > CLK_IDLE_TIMEOUT && cs_state == 0) {
                        GPIO.out_w1ts = P1_CS_OUT_MASK;
                        cs_state = 1;
                        break;
                    }
                }
                if (cs_state == 1) {
                    delay_us(350);
                    GPIO.out_w1tc = P1_CS_OUT_MASK;
                    cs_state = 0;
                    idx = 0;
                    for (uint32_t i = 0; i < 16; i++) {
                        SPI2.data_buf[i] = 0xFFFFFFFF;
                    }
                    load_buffer();
                    SPI2.slave.sync_reset = 1;
                    SPI2.slave.trans_done = 0;
                    SPI2.cmd.usr = 1;
                }
            }
        }
    }
}

void real_spi_init(void) {
    gpio_config_t io_conf = {0};

    /* CS Generator loopback */
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = 1ULL << P1_CS_OUT_PIN;
    gpio_config_iram(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = 1ULL << P1_CS_IN_PIN;
    gpio_config_iram(&io_conf);
    gpio_matrix_in(P1_CS_IN_PIN, HSPICS0_IN_IDX, false);

    /* RXD */
    gpio_set_level_iram(P1_RXD_PIN, 1);
    gpio_set_direction_iram(P1_RXD_PIN, GPIO_MODE_OUTPUT);
    gpio_matrix_out(P1_RXD_PIN, HSPIQ_OUT_IDX, false, false);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[P1_RXD_PIN], PIN_FUNC_GPIO);

    /* SCK */
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = 1ULL << P1_SCK_PIN;
    gpio_config_iram(&io_conf);
    gpio_matrix_in(P1_SCK_PIN, HSPICLK_IN_IDX, false);

    periph_ll_enable_clk_clear_rst(PERIPH_HSPI_MODULE);

    SPI2.clock.val = 0;
    SPI2.user.val = 0;
    SPI2.ctrl.val = 0;
    SPI2.slave.wr_rd_buf_en = 1; //no sure if needed
    SPI2.user.doutdin = 1; //we only support full duplex
    SPI2.user.sio = 0;
    SPI2.slave.slave_mode = 1;
    SPI2.dma_conf.val |= SPI_LL_RST_MASK;
    SPI2.dma_out_link.start = 0;
    SPI2.dma_in_link.start = 0;
    SPI2.dma_conf.val &= ~SPI_LL_RST_MASK;
    SPI2.slave.sync_reset = 1;
    SPI2.slave.sync_reset = 0;

    //use all 64 bytes of the buffer
    SPI2.user.usr_miso_highpart = 0;
    SPI2.user.usr_mosi_highpart = 0;

    //Disable unneeded ints
    SPI2.slave.val &= ~SPI_LL_UNUSED_INT_MASK;

    SPI2.ctrl.wr_bit_order = 0;
    SPI2.ctrl.rd_bit_order = 0;

    /* Set Mode 3 as per ESP32 TRM, except ck_i_edge that need to be 1 for original PSX! */
    SPI2.pin.ck_idle_edge = 0;
    SPI2.user.ck_i_edge = 1;
    SPI2.ctrl2.miso_delay_mode = 1;
    SPI2.ctrl2.miso_delay_num = 0;
    SPI2.ctrl2.mosi_delay_mode = 0;
    SPI2.ctrl2.mosi_delay_num = 0;

    SPI2.slave.sync_reset = 1;
    SPI2.slave.sync_reset = 0;

    SPI2.slv_wrbuf_dlen.bit_len = 0;
    SPI2.slv_rdbuf_dlen.bit_len = 1600 - 1;

    SPI2.user.usr_miso = 1;
    SPI2.user.usr_mosi = 1;

    SPI2.slave.trans_inten = 0;
    SPI2.slave.trans_done = 0;
    SPI2.cmd.usr = 1;

    cs_generator();
}
