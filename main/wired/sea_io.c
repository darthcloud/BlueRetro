/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/io_mux_reg.h"
#include <hal/clk_gate_ll.h>
#include "soc/rmt_struct.h"
#include <driver/rmt.h>
#include <hal/rmt_ll.h>
#include "adapter/adapter.h"
#include "system/gpio.h"
#include "zephyr/types.h"
#include "tools/util.h"

#define GBAHD_COM_PIN 33
#define GBAHD_BIT_PERIOD_TICKS 24
#define BIT_ZERO 0x80080010
#define BIT_ONE  0x80100008
#define BIT_STOP 0x80008000

static const uint8_t output_list[] = {
    4, 5, 12, 13, 14, 15, 16, 18, 19, 21, 22, 23
};
static volatile rmt_item32_t *rmt_items = (volatile rmt_item32_t *)RMTMEM.chan[0].data32;

void sea_tx_byte(uint8_t data) {
    volatile uint32_t *item_ptr = &rmt_items[0].val;

    for (uint32_t mask = 0x80; mask; mask >>= 1, ++item_ptr) {
        if (data & mask) {
            *item_ptr = BIT_ONE;
        }
        else {
            *item_ptr = BIT_ZERO;
        }
    }
    *item_ptr = BIT_STOP;
    rmt_tx_start(0, 1);
}

void sea_init(void) {
    /* SEA specific build required */
    gpio_config_t io_conf = {0};

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT_OD;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    /* Setup GBA buttons IO */
    for (uint32_t i = 0; i < ARRAY_SIZE(output_list); i++) {
        io_conf.pin_bit_mask = 1ULL << output_list[i];
        gpio_config_iram(&io_conf);
        gpio_set_level_iram(output_list[i], 1);
    }

    /* Setup RMT peripheral for GBAHD comport */
    periph_ll_enable_clk_clear_rst(PERIPH_RMT_MODULE);

    RMT.apb_conf.fifo_mask = RMT_DATA_MODE_MEM;
    RMT.conf_ch[0].conf0.div_cnt = 40; /* 80MHz (APB CLK) / 40 = 0.5us TICK */;
    RMT.conf_ch[0].conf1.mem_rd_rst = 1;
    RMT.conf_ch[0].conf1.mem_wr_rst = 1;
    RMT.conf_ch[0].conf1.tx_conti_mode = 0;
    RMT.conf_ch[0].conf0.mem_size = 8;
    RMT.conf_ch[0].conf1.mem_owner = RMT_MEM_OWNER_TX;
    RMT.conf_ch[0].conf1.ref_always_on = RMT_BASECLK_APB;
    RMT.conf_ch[0].conf1.idle_out_en = 1;
    RMT.conf_ch[0].conf1.idle_out_lv = RMT_IDLE_LEVEL_HIGH;
    RMT.conf_ch[0].conf0.carrier_en = 0;
    RMT.conf_ch[0].conf0.carrier_out_lv = RMT_CARRIER_LEVEL_LOW;
    RMT.carrier_duty_ch[0].high = 0;
    RMT.carrier_duty_ch[0].low = 0;
    RMT.conf_ch[0].conf0.idle_thres = GBAHD_BIT_PERIOD_TICKS;
    RMT.conf_ch[0].conf1.rx_filter_thres = 0; /* No minimum length */
    RMT.conf_ch[0].conf1.rx_filter_en = 0;

    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[GBAHD_COM_PIN], PIN_FUNC_GPIO);
    gpio_set_direction_iram(GBAHD_COM_PIN, GPIO_MODE_OUTPUT);
    gpio_matrix_out(GBAHD_COM_PIN, RMT_SIG_OUT0_IDX, 0, 0);
    gpio_matrix_in(GBAHD_COM_PIN, RMT_SIG_IN0_IDX, 0);

    /* No RX, just set in good state */
    rmt_ll_rx_enable(&RMT, 0, 0);
    rmt_ll_rx_reset_pointer(&RMT, 0);
    rmt_ll_clear_rx_end_interrupt(&RMT, 0);
}
