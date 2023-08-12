/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "jvs_uart.h"
#include "sdkconfig.h"
#if defined (CONFIG_BLUERETRO_SYSTEM_JVS)
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
#include "adapter/wired/jvs.h"
#include "system/delay.h"
#include "system/gpio.h"
#include "system/intr.h"

#define UART_DEV 1
#define JVS_TX_PIN 22
#define JVS_RX_PIN 21
#define JVS_RTS_PIN 19
#define JVS_SENSE_PIN 25
#define JVS_TX_MASK (1U << JVS_TX_PIN)
#define JVS_RX_MASK (1U << JVS_RX_PIN)
#define JVS_RTS_MASK (1U << JVS_RTS_PIN)
#define JVS_SENSE_MASK (1U << JVS_SENSE_PIN)

#define SYNC 0xE0
#define MARK 0xD0
#define E0BYTE 0xDF
#define D0BYTE 0xCF
#define BCAST 0xFF

static const uint8_t ident_str[] = "BlueRetro;JVS;ver1.0 ";
static uint8_t rx_buf[128];
static uint8_t tx_buf[128];
static uint8_t node_id = 0;

static inline uint32_t jvs_read_rxfifo(uint8_t *buf, uint32_t raw_len, uint32_t *len)
{
    uint8_t sum = 0;
    *len = 0;

    for (uint32_t i = 0; i < (raw_len - 1); ++i) {
        buf[*len] = READ_PERI_REG(UART_FIFO_REG(1));
        if (buf[*len] == SYNC) {
            *len = 0;
            sum = 0;
        }
        else {
            if (buf[*len] == MARK) {
                buf[*len] = READ_PERI_REG(UART_FIFO_REG(1));
                switch (buf[*len]) {
                    case E0BYTE:
                        buf[*len] = SYNC;
                        break;
                    case D0BYTE:
                        buf[*len] = MARK;
                }
                ++i;
            }
            sum += buf[*len];
            ++*len;
        }
    }
    return ((sum % 256) == READ_PERI_REG(UART_FIFO_REG(1)));
}

static inline void jvs_write_txfifo(const uint8_t *buf, uint32_t len)
{
    uint8_t sum = 0;

    WRITE_PERI_REG(UART_FIFO_AHB_REG(1), SYNC);
    for (uint32_t i = 0; i < len; ++i) {
        switch (buf[i]) {
            case SYNC:
                WRITE_PERI_REG(UART_FIFO_AHB_REG(1), MARK);
                WRITE_PERI_REG(UART_FIFO_AHB_REG(1), E0BYTE);
                break;
            case MARK:
                WRITE_PERI_REG(UART_FIFO_AHB_REG(1), MARK);
                WRITE_PERI_REG(UART_FIFO_AHB_REG(1), D0BYTE);
                break;
            default:
                WRITE_PERI_REG(UART_FIFO_AHB_REG(1), buf[i]);
        }
        sum += buf[i];
    }
    WRITE_PERI_REG(UART_FIFO_AHB_REG(1), (sum % 256));
}

static void jvs_parser(uint8_t *rx_buf, uint32_t rx_len, uint8_t *tx_buf, uint32_t *tx_len) {
    uint8_t *end = rx_buf + rx_len;
    uint8_t *jvs = rx_buf;
    uint8_t len = 0;

    if (*jvs == node_id || *jvs == BCAST) {
        jvs += 2;
        tx_buf[len++] = 0x00;
        len++;
        tx_buf[len++] = 0x01;
        while (jvs < end) {
            switch (*jvs++) {
                case 0xF0: /* Reset */
                    if (*jvs++ == 0xD9) {
                        GPIO.out_w1ts = JVS_SENSE_MASK;
                        node_id = 0;
                    }
                    len = 0;
                    break;
                case 0xF1: /* Set Address */
                    if (!node_id) {
                        node_id = *jvs;
                        GPIO.out_w1tc = JVS_SENSE_MASK;
                        tx_buf[len++] = 0x01;
                    }
                    else {
                        len = 0;
                    }
                    jvs++;
                    break;
                case 0xF2: /* Set Speed */
                    ets_printf("0xF2 NA\n");
                    jvs += 2;
                    break;
                case 0x10: /* Get Info */
                    tx_buf[len++] = 0x01;
                    memcpy(&tx_buf[4], ident_str, sizeof(ident_str));
                    len += sizeof(ident_str);
                    break;
                case 0x11: /* Get CMD Rev */
                    tx_buf[len++] = 0x01;
                    tx_buf[len++] = 0x13;
                    break;
                case 0x12: /* Get JVS Rev */
                    tx_buf[len++] = 0x01;
                    tx_buf[len++] = 0x30;
                    break;
                case 0x13: /* Get COMM Rev */
                    tx_buf[len++] = 0x01;
                    tx_buf[len++] = 0x10;
                    break;
                case 0x14: /* Get Feature */
                    tx_buf[len++] = 0x01;

                    /* Switch input */
                    tx_buf[len++] = 0x01;
                    tx_buf[len++] = 12;
                    tx_buf[len++] = 16;
                    tx_buf[len++] = 0x00;

                    /* Coin input */
                    tx_buf[len++] = 0x02;
                    tx_buf[len++] = 12;
                    tx_buf[len++] = 0x00;
                    tx_buf[len++] = 0x00;

                    /* Analog input */
                    tx_buf[len++] = 0x03;
                    tx_buf[len++] = 2 * 12;
                    tx_buf[len++] = 16;
                    tx_buf[len++] = 0x00;

                    /* End */
                    tx_buf[len++] = 0x00;
                    break;
                case 0x15: /* Set Info */
                    ets_printf("%s\n", jvs);
                    while (*jvs++ != 0);
                    tx_buf[len++] = 0x01;
                    break;
                case 0x20: /* Get Switch */
                {
                    uint8_t player_cnt = *jvs++;
                    uint8_t data_cnt = *jvs++;
                    tx_buf[len++] = 0x01;
                    tx_buf[len++] = wired_adapter.data[0].output[8];
                    for (uint32_t i = 0; i < player_cnt; i++) {
                        for (uint32_t j = 0; j < data_cnt; j++) {
                            tx_buf[len + j] = wired_adapter.data[i].output[2 + j] & wired_adapter.data[i].output_mask[2 + j];
                        }
                        ++wired_adapter.data[i].frame_cnt;
                        jvs_gen_turbo_mask(&wired_adapter.data[i]);
                        len += data_cnt;
                    }
                    break;
                }
                case 0x21: /* Get Coin */
                {
                    uint8_t slot_cnt = *jvs++;
                    tx_buf[len++] = 0x01;
                    for (uint32_t i = 0; i < slot_cnt; i++) {
                        *(uint16_t *)&tx_buf[len] = *(uint16_t *)wired_adapter.data[i].output;
                        len += 2;
                    }
                    break;
                }
                case 0x22: /* Get Analog */
                {
                    uint8_t ch_cnt = *jvs++;
                    tx_buf[len++] = 0x01;
                    for (uint32_t i = 0; i < ch_cnt; i++) {
                        if (*(uint16_t *)(wired_adapter.data[i / 2].output_mask + ((i & 0x1) ? 6 : 4))) {
                            *(uint16_t *)&tx_buf[len] = *(uint16_t *)(wired_adapter.data[i / 2].output_mask + ((i & 0x1) ? 6 : 4));
                        }
                        else {
                            *(uint16_t *)&tx_buf[len] = *(uint16_t *)(wired_adapter.data[i / 2].output + ((i & 0x1) ? 6 : 4));
                        }
                        len += 2;
                    }
                    break;
                }
                case 0x2F: /* Retransmit */
                    /* Last frame should still be in tx_buf */
                    len = tx_buf[1] + 1;
                    break;
                default:
                    /* Unsupported cmd, discard everything and return error */
                    ets_printf("0x%02X NA\n", *jvs);
                    tx_buf[1] = 0x03;
                    tx_buf[2] = 0x02;
                    tx_buf[3] = 0x01;
                    *tx_len = 4;
                    return;
            }
        }
        tx_buf[1] = len - 1;
    }
    if (len) {
        *tx_len = len;
    }
    else {
        *tx_len = 0;
    }
}

static unsigned uart_rx(unsigned cause) {
    uint32_t intr_status = UART1.int_st.val;

    if (intr_status & UART_INTR_RXFIFO_TOUT) {
        uint32_t rx_len, tx_len;
        uint16_t read_len = uart_ll_get_rxfifo_len(&UART1);

        if (!jvs_read_rxfifo(rx_buf, read_len, &rx_len)) {
            ets_printf("BAD\n");
        }
#ifdef CONFIG_BLUERETRO_WIRED_TRACE
        if (rx_len) {
            for (uint32_t i = 0; i < rx_len; ++i) {
                ets_printf("%02X", rx_buf[i]);
            }
            ets_printf("\n");
        }
        else {
            ets_printf("T\n");
        }
#else
        jvs_parser(rx_buf, rx_len, tx_buf, &tx_len);
#endif

        if (tx_len) {
            GPIO.out_w1ts = JVS_RTS_MASK;
            delay_us(10);
            jvs_write_txfifo(tx_buf, tx_len);
        }
    }
    if (intr_status & UART_INTR_RXFIFO_OVF) {
        ets_printf("RXFIFO_OVF\n");
        uart_ll_rxfifo_rst(&UART1);
    }
    if (intr_status & UART_INTR_TX_DONE) {
        GPIO.out_w1tc = JVS_RTS_MASK;
    }
    UART1.int_clr.val = intr_status;
    return 0;
}
#endif /* defined (CONFIG_BLUERETRO_SYSTEM_JVS */

void jvs_init(uint32_t package) {
#if defined (CONFIG_BLUERETRO_SYSTEM_JVS)
    gpio_config_t jvs_sense_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = JVS_SENSE_MASK,
    };
    gpio_config_t jvs_rts_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = JVS_RTS_MASK,
    };

    periph_ll_enable_clk_clear_rst(PERIPH_UART1_MODULE);

    /* JVS_SENSE is output */
    gpio_config_iram(&jvs_sense_conf);
    GPIO.out_w1ts = JVS_SENSE_MASK;

    /* JVS_RTS is output */
    gpio_config_iram(&jvs_rts_conf);
    GPIO.out_w1tc = JVS_RTS_MASK;

    /* JVS_TX is output */
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[JVS_TX_PIN], PIN_FUNC_GPIO);
    gpio_set_direction_iram(JVS_TX_PIN, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level_iram(JVS_TX_PIN, 1);
    gpio_matrix_out(JVS_TX_PIN, U1TXD_OUT_IDX, false, false);

    /* JVS_RX is input */
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[JVS_RX_PIN], PIN_FUNC_GPIO);
    gpio_set_pull_mode_iram(JVS_RX_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction_iram(JVS_RX_PIN, GPIO_MODE_INPUT);
    gpio_matrix_in(JVS_RX_PIN, U1RXD_IN_IDX, false);

    //Configure UART
    UART1.int_ena.val &= (~0x7ffff);
    UART1.int_clr.val = 0x7ffff;
    UART1.conf1.rx_tout_thrhd = 2; /* ~150us */
    UART1.conf1.rx_tout_en = 1;
    UART1.conf1.rxfifo_full_thrhd = 120;
    UART1.conf1.txfifo_empty_thrhd = 10;
    UART1.int_ena.val |= UART_INTR_RXFIFO_TOUT | UART_INTR_RXFIFO_OVF | UART_INTR_TX_DONE;
    UART1.clk_div.div_int = ((APB_CLK_FREQ << 4) / 115200) >> 4;
    UART1.clk_div.div_frag = ((APB_CLK_FREQ << 4) / 115200) & 0xF;
    UART1.conf0.tick_ref_always_on = 1;

    /* We deal with the RS485 stuff ourself */
    UART1.rs485_conf.en = 0;
    UART1.rs485_conf.tx_rx_en = 0;
    UART1.rs485_conf.rx_busy_tx_en = 0;
    UART1.conf0.irda_en = 0;

    UART1.conf0.parity_en = 0;
    UART1.conf0.bit_num = UART_DATA_8_BITS;
    UART1.rs485_conf.dl1_en = 0;
    UART1.conf0.stop_bit_num = UART_STOP_BITS_1;
    UART1.idle_conf.tx_idle_num = 0;
    UART1.conf1.rx_flow_en = 0;
    UART1.conf0.tx_flow_en = 0;

    uart_ll_rxfifo_rst(&UART1);
    uart_ll_txfifo_rst(&UART1);

    intexc_alloc_iram(ETS_UART1_INTR_SOURCE, 19, uart_rx);
#endif /* defined (CONFIG_BLUERETRO_SYSTEM_JVS */
}
