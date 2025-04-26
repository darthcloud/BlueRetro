/*
 * Copyright (c) 2024-2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <gcov.h>
#include "coverage.h"

#define COV_TX_PIN 12
#define COV_RX_PIN 13
#define COV_RTS_PIN 15
#define COV_CTS_PIN 14

/* The start and end symbols are provided by the linker script.  We use the
   array notation to avoid issues with a potential small-data area.  */
extern const struct gcov_info *const __gcov_info_start[];
extern const struct gcov_info *const __gcov_info_end[];
    
static int uart_port = UART_NUM_1;

/* This function shall produce a reliable in order byte stream to transfer the
   gcov information from the target to the host system.  */
static void dump(const void *d, unsigned n, void *arg) {
    (void)arg;
    uart_write_bytes(uart_port, d, n);
}

/* The filename is serialized to a gcfn data stream by the
    __gcov_filename_to_gcfn() function.  The gcfn data is used by the
    "merge-stream" subcommand of the "gcov-tool" to figure out the filename
    associated with the gcov information. */
static void filename(const char *f, void *arg) {
    __gcov_filename_to_gcfn(f, dump, arg);
}

/* The __gcov_info_to_gcda() function may have to allocate memory under
    certain conditions.  Simply try it out if it is needed for your application
    or not.  */
static void *allocate(unsigned length, void *arg) {
    (void)arg;
    return malloc(length);
}

/* Dump the gcov information of all translation units.  */
void cov_dump_info(void) {
    const struct gcov_info *const *info = __gcov_info_start;
    const struct gcov_info *const *end = __gcov_info_end;

    /* Obfuscate variable to prevent compiler optimizations.  */
    __asm__("" : "+r" (info));

    while (info != end)
    {
        void *arg = NULL;
        __gcov_info_to_gcda(*info, filename, dump, allocate, arg);
        ++info;
    }
}

void cov_init(void) {
    uart_config_t uart_cfg = {
        .baud_rate = 921600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
        .rx_flow_ctrl_thresh = UART_HW_FIFO_LEN(UART_NUM_1) - 1,
    };

    printf("# %s: set uart pin tx:%d\n", __FUNCTION__, COV_TX_PIN);
    printf("# %s: set baud_rate:%d.\n", __FUNCTION__, uart_cfg.baud_rate);

    ESP_ERROR_CHECK(uart_driver_delete(uart_port));
    ESP_ERROR_CHECK(uart_driver_install(uart_port, UART_HW_FIFO_LEN(port_num) * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(uart_port, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(uart_port, COV_TX_PIN, COV_RX_PIN, COV_RTS_PIN, COV_CTS_PIN));
}