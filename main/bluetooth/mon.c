/*
 * Copyright (c) 2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_timer.h>
#include "mon.h"
#include "host.h"
#include "zephyr/hci.h"

#define BT_MON_TX_PIN 12
#define BT_MON_RX_PIN 13
#define BT_MON_RTS_PIN 15
#define BT_MON_CTS_PIN 14

struct bt_mon_hdr {
	uint16_t data_len;
	uint16_t opcode;
	uint8_t flags;
	uint8_t hdr_len;
	uint8_t ts_type;
	uint32_t ts_data;
} __packed;

static int uart_port;
static struct bt_mon_hdr mon_hdr = {0};

int bt_mon_init(int port_num, int32_t baud_rate, uint8_t data_bits, uint8_t stop_bits,
    uart_parity_t parity, uart_hw_flowcontrol_t flow_ctl) {
    uart_config_t uart_cfg = {
        .baud_rate = baud_rate,
        .data_bits = data_bits,
        .parity = parity,
        .stop_bits = stop_bits,
        .flow_ctrl = flow_ctl,
        .source_clk = UART_SCLK_DEFAULT,
        .rx_flow_ctrl_thresh = UART_HW_FIFO_LEN(port_num) - 1,
    };
    uart_port = port_num;

    printf("# %s: set uart pin tx:%d\n", __FUNCTION__, BT_MON_TX_PIN);
    printf("# %s: set baud_rate:%ld.\n", __FUNCTION__, baud_rate);

    ESP_ERROR_CHECK(uart_driver_delete(port_num));
    ESP_ERROR_CHECK(uart_driver_install(port_num, UART_HW_FIFO_LEN(port_num) * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(port_num, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(port_num, BT_MON_TX_PIN, BT_MON_RX_PIN, BT_MON_RTS_PIN, BT_MON_CTS_PIN));

    return 0;
}

void IRAM_ATTR bt_mon_tx(uint16_t opcode, uint8_t *data, uint16_t len) {
    mon_hdr.data_len = len + 4 + 5;
    mon_hdr.opcode = opcode;
    mon_hdr.hdr_len = 5;
    mon_hdr.ts_type = 8;
    mon_hdr.ts_data = esp_timer_get_time() / 100;

    uart_write_bytes(uart_port, &mon_hdr, sizeof(mon_hdr));
    uart_write_bytes(uart_port, data, len);
}
