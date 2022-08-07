/*
 * Copyright (c) 2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <esp_heap_caps.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "system/fs.h"
#include "fpga_config.h"

#define FPGA_PROGRAM_B_PIN 25
#define FPGA_INIT_B_PIN 26
#define FPGA_CCLK_PIN 17
#define FPGA_DIN_PIN 27
#define FPGA_DONE_PIN 34

#define BUFFER_SIZE (4 * 1024)

int32_t fpga_config(void) {
    int32_t ret = -1;
    uint8_t *buffer = NULL;
    uint32_t block_cnt, end_size;
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    spi_device_handle_t spi;
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = FPGA_DIN_PIN,
        .sclk_io_num = FPGA_CCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BUFFER_SIZE,
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
    };
    struct spi_transaction_t transcfg = {0};
    struct stat st;

    if (stat(BITSTREAM_FILE, &st) != 0) {
        printf("# %s: No bitstream file found\n", __FUNCTION__);
        return ret;
    }
    block_cnt = st.st_size / BUFFER_SIZE;
    end_size = st.st_size % BUFFER_SIZE;
    printf("# %s: Bitstream file found! size: (4KB * %ld) + %ld\n", __FUNCTION__, block_cnt, end_size);

    gpio_set_level(FPGA_PROGRAM_B_PIN, 1);
    io_conf.pin_bit_mask = 1ULL << FPGA_PROGRAM_B_PIN;
    (void)gpio_config(&io_conf);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << FPGA_INIT_B_PIN;
    (void)gpio_config(&io_conf);
    io_conf.pin_bit_mask = 1ULL << FPGA_DONE_PIN;
    (void)gpio_config(&io_conf);

    buffer = malloc(BUFFER_SIZE);

    if (buffer == NULL) {
        printf("# %s: buffer alloc fail\n", __FUNCTION__);
        heap_caps_dump_all();
        return ret;
    }

    ret = spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret) {
        printf("# %s: spi bus init fail\n", __FUNCTION__);
        goto release_mem;
    }

    ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    if (ret) {
        goto release_bus;
    }

    gpio_set_level(FPGA_PROGRAM_B_PIN, 0);
    /* Wait for INIT_B to get low */
    while(gpio_get_level(FPGA_INIT_B_PIN));
    gpio_set_level(FPGA_PROGRAM_B_PIN, 1);
    /* Wait for INIT_B to get back up */
    while(!gpio_get_level(FPGA_INIT_B_PIN));
    printf("# %s: FPGA is reset\n", __FUNCTION__);

    FILE *file = fopen(BITSTREAM_FILE, "rb");
    if (file == NULL) {
        printf("# %s: failed to open file for reading\n", __FUNCTION__);
    }
    else {
        uint32_t count = 0;
        uint32_t total_cnt = 0;

        printf("\n");

        /* Write in 4KB block */
        while (total_cnt < block_cnt) {
            count = fread(buffer, BUFFER_SIZE, 1, file);
            if (count) {
                transcfg.rxlength = 0;
                transcfg.length = BUFFER_SIZE * 8;
                transcfg.tx_buffer = buffer;
                if (spi_device_transmit(spi, &transcfg)) {
                    printf("# %s: SPI TX fail block: %ld\n", __FUNCTION__, total_cnt);
                    goto close_file;
                }
                total_cnt++;
                printf("\r# %s: Uploading bitstream to FPGA... %ld%%", __FUNCTION__, 100 * total_cnt / block_cnt);
            }
        }

        /* Get remainder of the file */
        if (end_size) {
            do {
                count = fread(buffer, end_size, 1, file);
            } while (count == 0);

            transcfg.rxlength = 0;
            transcfg.length = end_size * 8;
            transcfg.tx_buffer = buffer;
            if (spi_device_transmit(spi, &transcfg)) {
                printf("# %s: SPI TX fail block: %ld\n", __FUNCTION__, total_cnt);
                goto close_file;
            }
        }
        printf("\r# %s: Uploading bitstream to FPGA... 100%%\n", __FUNCTION__);
        printf("# %s: Bitstream upload done! 4KB block: %ld last block size: %ld\n", __FUNCTION__, total_cnt, end_size);
close_file:
        fclose(file);
    }

    (void)spi_bus_remove_device(spi);
release_bus:
    (void)spi_bus_free(HSPI_HOST);
release_mem:
    if (buffer) {
        free(buffer);
    }
    printf("# %s: Pin state: DONE: %d INIT: %d\n", __FUNCTION__, gpio_get_level(FPGA_DONE_PIN), gpio_get_level(FPGA_INIT_B_PIN));
    return ret;
}
