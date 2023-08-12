/*
 * Copyright (c) 2021-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <hal/clk_gate_ll.h>
#include <soc/uart_periph.h>
#include <hal/uart_ll.h>
#include <esp32/rom/ets_sys.h>
#include <esp32/rom/gpio.h>
#include "esp_private/esp_clk.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "system/delay.h"
#include "system/gpio.h"
#include "system/intr.h"
#include "adapter/kb_monitor.h"
#include "adapter/wired/cdi.h"
#include "cdi_uart.h"

#define UART1_INTR_NUM 19
#define UART2_INTR_NUM 20
#define GPIO_INTR_NUM 21

#define UART1_INTR_MASK (1 << UART1_INTR_NUM)
#define UART2_INTR_MASK (1 << UART2_INTR_NUM)
#define GPIO_INTR_MASK (1 << GPIO_INTR_NUM)


#define P1_RTS_PIN 23
#define P1_DATA_PIN 22
#define P2_RTS_PIN 21
#define P2_DATA_PIN 19

#define P1_RTS_MASK (1 << P1_RTS_PIN)
#define P1_DATA_MASK (1 << P1_DATA_PIN)
#define P2_RTS_MASK (1 << P2_RTS_PIN)
#define P2_DATA_MASK (1 << P2_DATA_PIN)

#define CDI_PORT_MAX 2

struct cdi_ctrl_port {
    uint8_t buffer[4];
    uart_dev_t *hw;
    uint32_t rts_pin;
    uint32_t data_pin;
    uint32_t rts_mask;
    uint32_t data_mask;
    uint8_t data_sig;
    uint8_t uart_mod;
    uint8_t id_code;
    uint8_t tx_in_progress;
};

static struct cdi_ctrl_port cdi_ctrl_ports[CDI_PORT_MAX] = {
    {
        .hw = &UART1,
        .rts_pin = P1_RTS_PIN,
        .data_pin = P1_DATA_PIN,
        .rts_mask = P1_RTS_MASK,
        .data_mask = P1_DATA_MASK,
        .data_sig = U1TXD_OUT_IDX,
        .uart_mod = PERIPH_UART1_MODULE,
        .tx_in_progress = 0,
    },
    {
        .hw = &UART2,
        .rts_pin = P2_RTS_PIN,
        .data_pin = P2_DATA_PIN,
        .rts_mask = P2_RTS_MASK,
        .data_mask = P2_DATA_MASK,
        .data_sig = U2TXD_OUT_IDX,
        .uart_mod = PERIPH_UART2_MODULE,
        .tx_in_progress = 0,
    },
};

static inline uint8_t load_data(uint8_t port, uint8_t *data) {
    uint8_t *relative = (uint8_t *)(wired_adapter.data[port].output + 6);
    int32_t *raw_axes = (int32_t *)(wired_adapter.data[port].output + 8);
    int32_t *raw_axes_mask = (int32_t *)(wired_adapter.data[port].output_mask + 8);
    int32_t val = 0;
    uint8_t tmp = 0;
    uint8_t move = 0;

    data[0] = wired_adapter.data[port].output[0] & wired_adapter.data[port].output_mask[0];

    for (uint32_t i = 0; i < 2; i++) {
        if (relative[i]) {
            val = atomic_clear(&raw_axes[i]);
        }
        else {
            val = raw_axes[i] & raw_axes_mask[i];
        }

        if (val > 127) {
            tmp = 127;
        }
        else if (val < -128) {
            tmp = -128;
        }
        else {
            tmp = (uint8_t)val;
        }

        switch (i) {
            case 0:
                data[0] &= ~0x03;
                data[0] |= ((tmp & 0xC0) >> 6);
                data[1] &= ~0x3F;
                data[1] |= (tmp & 0x3F);
                break;
            case 1:
                data[0] &= ~0x0C;
                data[0] |= ((tmp & 0xC0) >> 4);
                data[2] &= ~0x3F;
                data[2] |= (tmp & 0x3F);
                break;
        }

        if (val) {
            move = 1;
        }
    }
    return move;
}

static inline uint8_t load_k_kb_data(uint8_t port, uint8_t *data) {
    uint32_t len = 0;
    uint8_t tmp[3] = {0};

    if (kbmon_get_code(port, tmp, &len)) {
        return 0;
    }

    data[0] = 0x80 | ((tmp[0] & 0xF) << 3) | ((tmp[1] & 0x3) << 1) | ((tmp[2] & 0x80) >> 7);
    data[1] = tmp[2] & 0x7F;
    return 1;
}

static inline uint8_t load_t_kb_data(uint8_t port, uint8_t *data) {
    uint32_t len = 0;
    uint8_t tmp[3] = {0};

    if (kbmon_get_code(port, tmp, &len)) {
        return 0;
    }

    data[0] = 0x40;
    data[1] = 0x10 | ((tmp[0] & 0x0C) >> 2);
    data[2] = ((tmp[0] & 0x3) << 4) | ((tmp[1] & 0x3) << 2) | ((tmp[2] & 0xC0) >> 6);
    data[3] = tmp[2] & 0x3F;
    return 1;
}

static void rts_isr(void) {
    const uint32_t low_io = GPIO.acpu_int;
    struct cdi_ctrl_port *p;

    for (uint32_t i = 0; i < CDI_PORT_MAX; i++) {
        p = &cdi_ctrl_ports[i];

        if (low_io & p->rts_mask) {
            if (GPIO.in & p->rts_mask) {
                uint8_t len;
                delay_us(2200);
                WRITE_PERI_REG(UART_FIFO_AHB_REG(i + 1), p->id_code);

                switch (p->id_code) {
                    case 'K':
                    case 'X':
                        len = 2;
                        break;
                    case 'T':
                        len = 4;
                        break;
                    default:
                        len = 3;
                        break;
                }

                for (uint32_t j = 0; j < len; j++) {
                    WRITE_PERI_REG(UART_FIFO_AHB_REG(i + 1), p->buffer[j]);
                }
            }
            else {
                p->hw->conf0.txfifo_rst = 1;
                p->hw->conf0.txfifo_rst = 0;
            }
        }
    }

    if (low_io) GPIO.status_w1tc = low_io;
}

static void uart_isr(struct cdi_ctrl_port *p) {
    uint32_t intr_status = p->hw->int_st.val;

    if (intr_status & UART_INTR_TX_DONE) {
        p->tx_in_progress = 0;
    }

    p->hw->int_clr.val = intr_status;
}

static void tx_hdlr(void) {

    while (1) {
        for (uint32_t i = 0; i < CDI_PORT_MAX; i++) {
            struct cdi_ctrl_port *p = &cdi_ctrl_ports[i];

            if (!p->tx_in_progress) {
                uint8_t tmp[4] = {0};
                uint8_t update, len;

                switch (p->id_code) {
                    case 'K':
                    case 'X':
                        update = load_k_kb_data(i, tmp);
                        len = 2;
                        break;
                    case 'T':
                        update = load_t_kb_data(i, tmp);
                        len = 4;
                        break;
                    default:
                        update = load_data(i, tmp);
                        len = 3;
                        if (p->buffer[0] != tmp[0]) {
                            update = 1;
                        }
                        ++wired_adapter.data[i].frame_cnt;
                        cdi_gen_turbo_mask(&wired_adapter.data[i]);
                        break;
                }

                if (update) {
                    memcpy(p->buffer, tmp, 4);
                    for (uint32_t j = 0; j < len; j++) {
                        if (GPIO.in & p->rts_mask) {
                            WRITE_PERI_REG(UART_FIFO_AHB_REG(i + 1), p->buffer[j]);
                        }
                    }
                    p->tx_in_progress = 1;
                }
            }
        }
    }
}

static unsigned isr_dispatch(unsigned cause) {
    if (cause & UART1_INTR_MASK) {
        uart_isr((void *)&cdi_ctrl_ports[0]);
    }
    if (cause & UART2_INTR_MASK) {
        uart_isr((void *)&cdi_ctrl_ports[1]);
    }
    if (cause & GPIO_INTR_MASK) {
        rts_isr();
    }
    return 0;
}

void cdi_uart_init(uint32_t package) {
    gpio_config_t io_conf = {0};

    for (uint32_t i = 0; i < CDI_PORT_MAX; i++) {
        struct cdi_ctrl_port *p = &cdi_ctrl_ports[i];

        switch (config.out_cfg[i].dev_mode) {
            case DEV_KB:
                /* Use acc cfg to choose KB type */
                switch (config.out_cfg[i].acc_mode) {
                    case ACC_MEM:
                        p->id_code = 'X';
                        break;
                    case ACC_RUMBLE:
                        p->id_code = 'T';
                        break;
                    default:
                        p->id_code = 'K';
                        break;
                }
                break;
            case DEV_MOUSE:
                p->id_code = 'M';
                break;
            default:
                p->id_code = 'J';
                break;
        }

        /* RTS */
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pin_bit_mask = 1ULL << p->rts_pin;
        gpio_config_iram(&io_conf);

        /* Data */
        gpio_set_direction_iram(p->data_pin, GPIO_MODE_OUTPUT);
        gpio_set_level_iram(p->data_pin, 1);
        gpio_matrix_out(p->data_pin, p->data_sig, false, false);
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[p->data_pin], PIN_FUNC_GPIO);

        periph_ll_enable_clk_clear_rst(p->uart_mod);

        //Configure UART
        p->hw->int_ena.val &= (~0x7ffff);
        p->hw->int_clr.val = 0x7ffff;
        p->hw->conf1.rx_tout_thrhd = 2; /* ~150us */
        p->hw->conf1.rx_tout_en = 0;
        p->hw->conf1.rxfifo_full_thrhd = 120;
        p->hw->conf1.txfifo_empty_thrhd = 10;
        p->hw->int_ena.val |= UART_INTR_TX_DONE;
        p->hw->clk_div.div_int = ((APB_CLK_FREQ << 4) / 1200) >> 4;
        p->hw->clk_div.div_frag = ((APB_CLK_FREQ << 4) / 1200) & 0xF;
        p->hw->conf0.tick_ref_always_on = 1;

        /* Disable RS485 */
        p->hw->rs485_conf.en = 0;
        p->hw->rs485_conf.tx_rx_en = 0;
        p->hw->rs485_conf.rx_busy_tx_en = 0;
        p->hw->conf0.irda_en = 0;

        if (p->id_code == 'K' || p->id_code == 'X') {
            p->hw->conf0.bit_num = UART_DATA_8_BITS;
            p->hw->conf0.stop_bit_num = UART_STOP_BITS_1;
        }
        else {
            p->hw->conf0.bit_num = UART_DATA_7_BITS;
            p->hw->conf0.stop_bit_num = UART_STOP_BITS_2;
        }

        p->hw->conf0.parity_en = 0;
        p->hw->rs485_conf.dl1_en = 0;
        p->hw->conf0.txd_inv = 1;
        p->hw->idle_conf.tx_idle_num = 0;
        p->hw->conf1.rx_flow_en = 0;
        p->hw->conf0.tx_flow_en = 0;

        uart_ll_rxfifo_rst(p->hw);
        uart_ll_txfifo_rst(p->hw);

        memcpy(p->buffer, wired_adapter.data[i].output, 4);
    }

    intexc_alloc_iram(ETS_UART1_INTR_SOURCE, UART1_INTR_NUM, isr_dispatch);
    intexc_alloc_iram(ETS_UART2_INTR_SOURCE, UART2_INTR_NUM, isr_dispatch);
    intexc_alloc_iram(ETS_GPIO_INTR_SOURCE, GPIO_INTR_NUM, isr_dispatch);

    tx_hdlr();
}
