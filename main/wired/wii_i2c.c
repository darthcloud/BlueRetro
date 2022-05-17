/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wii_i2c.h"
#include "sdkconfig.h"
#if defined (CONFIG_BLUERETRO_SYSTEM_WII_EXT) || defined(CONFIG_BLUERETRO_SYSTEM_UNIVERSAL)
#include <string.h>
#include "soc/io_mux_reg.h"
#include "esp_private/periph_ctrl.h"
#include <soc/i2c_periph.h>
#include <esp32/rom/ets_sys.h>
#include <esp32/rom/gpio.h>
#include "hal/i2c_ll.h"
#include "hal/clk_gate_ll.h"
#include "hal/misc.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "system/intr.h"
#include "system/gpio.h"
#include "system/delay.h"
#include "zephyr/atomic.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"

#define I2C0_INTR_NUM 19
#define I2C1_INTR_NUM 20

#define I2C0_INTR_MASK (1 << I2C0_INTR_NUM)
#define I2C1_INTR_MASK (1 << I2C1_INTR_NUM)

#define P1_SCL_PIN 25
#define P1_SDA_PIN 26
#define P2_SCL_PIN 5
#define P2_SDA_PIN 27

#define P1_SCL_MASK (1 << P1_SCL_PIN)
#define P1_SDA_MASK (1 << P1_SDA_PIN)
#define P2_SCL_MASK (1 << P2_SCL_PIN)
#define P2_SDA_MASK (1 << P2_SDA_PIN)

#define WII_PORT_MAX 2

struct wii_ctrl_port {
    i2c_dev_t *hw;
    uint32_t id;
    uint32_t scl_pin;
    uint32_t sda_pin;
    uint32_t scl_mask;
    uint32_t sda_mask;
    uint32_t fifo_addr;
    uint8_t sda_out_sig;
    uint8_t sda_in_sig;
    uint8_t scl_out_sig;
    uint8_t scl_in_sig;
    uint8_t module;
};

static struct wii_ctrl_port wii_ctrl_ports[WII_PORT_MAX] = {
    {
        .hw = &I2C0,
        .id = 0,
        .scl_pin = P1_SCL_PIN,
        .sda_pin = P1_SDA_PIN,
        .scl_mask = P1_SCL_MASK,
        .sda_mask = P1_SDA_MASK,
        .fifo_addr = 0x6001301c,
        .sda_out_sig = I2CEXT0_SDA_OUT_IDX,
        .sda_in_sig = I2CEXT0_SDA_IN_IDX,
        .scl_out_sig = I2CEXT0_SCL_OUT_IDX,
        .scl_in_sig = I2CEXT0_SCL_IN_IDX,
        .module = PERIPH_I2C0_MODULE,
    },
    {
        .hw = &I2C1,
        .id = 1,
        .scl_pin = P2_SCL_PIN,
        .sda_pin = P2_SDA_PIN,
        .scl_mask = P2_SCL_MASK,
        .sda_mask = P2_SDA_MASK,
        .fifo_addr = 0x6002701c,
        .sda_out_sig = I2CEXT1_SDA_OUT_IDX,
        .sda_in_sig = I2CEXT1_SDA_IN_IDX,
        .scl_out_sig = I2CEXT1_SCL_OUT_IDX,
        .scl_in_sig = I2CEXT1_SCL_IN_IDX,
        .module = PERIPH_I2C1_MODULE,
    },
};

static uint8_t wii_registers[] = {
    /* 0xF0 */ 0x00, 0x00, 0x00, 0x00,
    /* 0xF4 */ 0x00, 0x00, 0x00, 0x00,
    /* 0xF8 */ 0x00, 0x00, 0x00, 0x00,
    /* 0xFC */ 0xA4, 0x20, 0x01, 0x01,
};

static inline void write_fifo(struct wii_ctrl_port *port, const uint8_t *data, uint32_t len)
{
    port->hw->fifo_conf.tx_fifo_rst = 1;
    port->hw->fifo_conf.tx_fifo_rst = 0;

    for (int i = 0; i < len; i++) {
        WRITE_PERI_REG(port->fifo_addr, data[i]);
    }
}

static void i2c_isr(void* arg) {
    struct wii_ctrl_port *port = (struct wii_ctrl_port *)arg;
    uint32_t rx_fifo_cnt = port->hw->status_reg.rx_fifo_cnt;

    if (rx_fifo_cnt) {
        uint8_t reg = HAL_FORCE_READ_U32_REG_FIELD(port->hw->fifo_data, data);

        if (reg >= 0xF0) {
            reg &= 0xF;
            rx_fifo_cnt--;
            if (rx_fifo_cnt) {
                /* Write registers */
                for (uint32_t i = 0; i < rx_fifo_cnt; i++) {
                    uint8_t val = HAL_FORCE_READ_U32_REG_FIELD(port->hw->fifo_data, data);
                    wii_registers[reg] = val;
                    if (++reg > 0xF) {
                        break;
                    }
                }
            }
            else {
                /* Read registers */
                write_fifo(port, wii_registers + reg, sizeof(wii_registers) - reg);
            }
        }
        else if (reg == 0x00) {
            /* Controller status poll */
            write_fifo(port, wired_adapter.data[port->id].output, 21);
        }
    }

    port->hw->fifo_conf.rx_fifo_rst = 1;
    port->hw->fifo_conf.rx_fifo_rst = 0;
    port->hw->int_clr.val = I2C_LL_SLAVE_RX_INT;
}

static uint32_t isr_dispatch(uint32_t cause) {
    if (cause & I2C0_INTR_MASK) {
        i2c_isr((void *)&wii_ctrl_ports[0]);
    }
    if (cause & I2C1_INTR_MASK) {
        i2c_isr((void *)&wii_ctrl_ports[1]);
    }
    return 0;
}
#endif /* defined(CONFIG_BLUERETRO_SYSTEM_WII_EXT) || defined(CONFIG_BLUERETRO_SYSTEM_UNIVERSAL) */

void wii_i2c_init(void) {
#if defined (CONFIG_BLUERETRO_SYSTEM_WII_EXT) || defined(CONFIG_BLUERETRO_SYSTEM_UNIVERSAL)
    for (uint32_t i = 0; i < WII_PORT_MAX; i++) {
        struct wii_ctrl_port *p = &wii_ctrl_ports[i];

        /* Data */
        gpio_set_level_iram(p->sda_pin, 1);
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[p->sda_pin], PIN_FUNC_GPIO);
        gpio_set_direction_iram(p->sda_pin, GPIO_MODE_INPUT_OUTPUT_OD);
        gpio_set_pull_mode_iram(p->sda_pin, GPIO_PULLUP_ONLY);
        gpio_matrix_out(p->sda_pin, p->sda_out_sig, false, false);
        gpio_matrix_in(p->sda_pin, p->sda_in_sig, false);

        /* Clock */
        gpio_set_level_iram(p->scl_pin, 1);
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[p->scl_pin], PIN_FUNC_GPIO);
        gpio_set_direction_iram(p->scl_pin, GPIO_MODE_INPUT_OUTPUT_OD);
        gpio_matrix_out(p->scl_pin, p->scl_out_sig, false, false);
        gpio_matrix_in(p->scl_pin, p->scl_in_sig, false);
        gpio_set_pull_mode_iram(p->scl_pin, GPIO_PULLUP_ONLY);

        periph_ll_enable_clk_clear_rst(p->module);

        p->hw->int_ena.val &= I2C_LL_INTR_MASK;
        p->hw->int_clr.val = I2C_LL_INTR_MASK;
        p->hw->ctr.val = 0x3; /* Force SDA & SCL to 1 */
        p->hw->fifo_conf.fifo_addr_cfg_en = 0;
        p->hw->fifo_conf.nonfifo_en = 0;
        p->hw->ctr.tx_lsb_first = I2C_DATA_MODE_MSB_FIRST;
        p->hw->ctr.rx_lsb_first = I2C_DATA_MODE_MSB_FIRST;
        p->hw->fifo_conf.tx_fifo_rst = 1;
        p->hw->fifo_conf.tx_fifo_rst = 0;
        p->hw->fifo_conf.rx_fifo_rst = 1;
        p->hw->fifo_conf.rx_fifo_rst = 0;
        p->hw->slave_addr.addr = 0x70;
        p->hw->slave_addr.en_10bit = 0;
        p->hw->fifo_conf.rx_fifo_full_thrhd = 28;
        p->hw->fifo_conf.tx_fifo_empty_thrhd = 5;
        p->hw->sda_hold.time = 10;
        p->hw->sda_sample.time = 10;
        p->hw->timeout.tout = 32000;
        p->hw->int_ena.val |= I2C_LL_SLAVE_RX_INT;
    }

    intexc_alloc_iram(ETS_I2C_EXT0_INTR_SOURCE, I2C0_INTR_NUM, isr_dispatch);
    intexc_alloc_iram(ETS_I2C_EXT1_INTR_SOURCE, I2C1_INTR_NUM, isr_dispatch);

    wii_i2c_port_cfg(0x3);
#endif /* defined(CONFIG_BLUERETRO_SYSTEM_WII_EXT) || defined(CONFIG_BLUERETRO_SYSTEM_UNIVERSAL) */
}

void wii_i2c_port_cfg(uint16_t mask) {
#if defined (CONFIG_BLUERETRO_SYSTEM_WII_EXT) || defined(CONFIG_BLUERETRO_SYSTEM_UNIVERSAL)
    for (uint32_t i = 0; i < WII_PORT_MAX; i++) {
        struct wii_ctrl_port *p = &wii_ctrl_ports[i];

        if (mask & 0x1) {
            p->hw->slave_addr.addr = 0x52; /* WII ACC all use 0x52 */
        }
        else {
            p->hw->slave_addr.addr = 0x70; /* Use random addr to simply disable port */
        }
        mask >>= 1;
    }
#endif /* defined(CONFIG_BLUERETRO_SYSTEM_WII_EXT) || defined(CONFIG_BLUERETRO_SYSTEM_UNIVERSAL) */
}
