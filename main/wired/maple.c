/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <xtensa/hal.h>
#include <esp32/dport_access.h>
#include <esp_intr_alloc.h>
#include "driver/gpio.h"
#include "../zephyr/types.h"
#include "../util.h"
#include "../adapter/adapter.h"
#include "../adapter/config.h"
#include "maple.h"

#define ID_CTRL    0x00000001
#define ID_VMU_MEM 0x00000002
#define ID_VMU_LCD 0x00000004
#define ID_VMU_CLK 0x00000008
#define ID_MIC     0x00000010
#define ID_KB      0x00000040
#define ID_GUN     0x00000080
#define ID_RUMBLE  0x00000100
#define ID_MOUSE   0x00000200

#define CMD_INFO_REQ      0x01
#define CMD_EXT_INFO_REQ  0x02
#define CMD_RESET         0x03
#define CMD_SHUTDOWN      0x04
#define CMD_INFO_RSP      0x05
#define CMD_EXT_INFO_RSP  0x06
#define CMD_ACK           0x07
#define CMD_DATA_TX       0x08
#define CMD_GET_CONDITION 0x09
#define CMD_MEM_INFO_REQ  0x0A
#define CMD_BLOCK_READ    0x0B
#define CMD_BLOCK_WRITE   0x0C
#define CMD_SET_CONDITION 0x0E

#define CMD_SEND_SERIAL_STRING   0xA0

#define ADDR_MASK   0x3F
#define ADDR_CTRL   0x20
#define ADDR_MEM    0x01
#define ADDR_RUMBLE 0x02

#define DESC_CTRL     0x000F06FE
#define DESC_CTRL_ALT 0x003FFFFF
#define DESC_RUMBLE   0x01010000

//#define WIRED_TRACE
#define DEBUG  (1ULL << 25)
#define TIMEOUT 8

#define wait_100ns() asm("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
#define maple_fix_byte(s, a, b) (s ? ((a << s) | (b >> (8 - s))) : b)

uint32_t dummy_data[2];

struct maple_pkt {
    union {
        struct {
            uint8_t len;
            uint8_t src;
            uint8_t dst;
            uint8_t cmd;
            uint32_t data32[135];
        };
        uint8_t data[545];
    };
} __packed;

static const uint8_t gpio_pin[4][2] = {
    {21, 22},
    { 3,  5},
    {18, 23},
    {26, 27},
};

static uint8_t pin_to_port[] = {
    0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00,
};

static uint32_t maple0_to_maple1[] = {
    0x00, 0x00, 0x00, BIT(5), 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, BIT(23), 0x00, 0x00, BIT(22), 0x00, 0x00,
    0x00, 0x00, BIT(27), 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t ctrl_area_dir_name[] = {
    0x72, 0x44, 0x00, 0xFF, 0x63, 0x6D, 0x61, 0x65, 0x20, 0x74, 0x73, 0x61, 0x74, 0x6E, 0x6F, 0x43,
    0x6C, 0x6C, 0x6F, 0x72, 0x20, 0x20, 0x72, 0x65, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};

static const uint8_t rumble_area_dir_name[] = {
    0x72, 0x44, 0x00, 0xFF, 0x63, 0x6D, 0x61, 0x65, 0x20, 0x74, 0x73, 0x61, 0x74, 0x6E, 0x6F, 0x43,
    0x6C, 0x6C, 0x6F, 0x72, 0x20, 0x20, 0x72, 0x65, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};

static const uint8_t brand[] = {
    0x64, 0x6F, 0x72, 0x50, 0x64, 0x65, 0x63, 0x75, 0x20, 0x79, 0x42, 0x20, 0x55, 0x20, 0x72, 0x6F,
    0x72, 0x65, 0x64, 0x6E, 0x63, 0x69, 0x4C, 0x20, 0x65, 0x73, 0x6E, 0x65, 0x6F, 0x72, 0x46, 0x20,
    0x45, 0x53, 0x20, 0x6D, 0x45, 0x20, 0x41, 0x47, 0x52, 0x45, 0x54, 0x4E, 0x53, 0x49, 0x52, 0x50,
    0x4C, 0x2C, 0x53, 0x45, 0x20, 0x2E, 0x44, 0x54, 0x20, 0x20, 0x20, 0x20,
};

static struct maple_pkt pkt;
static uint32_t rumble_max = 0x00020013;
static uint32_t rumble_val = 0x10E0073B;

static void IRAM_ATTR maple_tx(uint32_t port, uint32_t maple0, uint32_t maple1, uint8_t *data, uint8_t len) {
    uint8_t *crc = data + (len - 1);
    *crc = 0x00;

    ets_delay_us(55);

    GPIO.out_w1ts = maple0 | maple1;
    gpio_set_direction(gpio_pin[port][0], GPIO_MODE_OUTPUT);
    gpio_set_direction(gpio_pin[port][1], GPIO_MODE_OUTPUT);
    DPORT_STALL_OTHER_CPU_START();
    GPIO.out_w1tc = maple0;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple1;
    wait_100ns();
    wait_100ns();

    for (uint32_t bit = 0; bit < len*8; ++data) {
        for (uint32_t mask = 0x80; mask; mask >>= 1, ++bit) {
            GPIO.out_w1ts = maple0;
            wait_100ns();
            wait_100ns();
            if (*data & mask) {
                GPIO.out_w1ts = maple1;
            }
            else {
                GPIO.out_w1tc = maple1;
            }
            wait_100ns();
            GPIO.out_w1tc = maple0;
            wait_100ns();
            wait_100ns();
            mask >>= 1;
            ++bit;
            GPIO.out_w1ts = maple1;
            wait_100ns();
            wait_100ns();
            if (*data & mask) {
                GPIO.out_w1ts = maple0;
            }
            else {
                GPIO.out_w1tc = maple0;
            }
            wait_100ns();
            GPIO.out_w1tc = maple1;
            wait_100ns();
            wait_100ns();
        }
        *crc ^= *data;
    }
    GPIO.out_w1ts = maple0;
    wait_100ns();
    GPIO.out_w1ts = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple0;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple0;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple0;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple0;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple1;

    gpio_set_direction(gpio_pin[port][0], GPIO_MODE_INPUT);
    gpio_set_direction(gpio_pin[port][1], GPIO_MODE_INPUT);
    DPORT_STALL_OTHER_CPU_END();
    /* Send start sequence */

}

static void IRAM_ATTR maple_rx(void* arg)
{
    const uint32_t maple0 = GPIO.acpu_int;
    uint32_t timeout;
    uint32_t bit_cnt = 0;
    uint32_t gpio;
    uint8_t *data = pkt.data;
#ifdef WIRED_TRACE
    uint32_t byte;
#endif
    uint32_t port;
    uint32_t bad_frame;
    uint8_t len, cmd, src, dst, crc = 0;
    uint32_t maple1;

    if (maple0) {
        DPORT_STALL_OTHER_CPU_START();
        maple1 = maple0_to_maple1[__builtin_ffs(maple0) - 1];
        while (1) {
            for (uint32_t mask = 0x80; mask; mask >>= 1, ++bit_cnt) {
                while (!(GPIO.in & maple0));
                while (((gpio = GPIO.in) & maple0));
                if (gpio & maple1) {
                    *data |= mask;
                }
                else {
                    *data &= ~mask;
                }
                mask >>= 1;
                ++bit_cnt;
                while (!(GPIO.in & maple1));
                timeout = 0;
                while (((gpio = GPIO.in) & maple1)) {
                    if (++timeout > TIMEOUT) {
                        goto maple_end;
                    }
                }
                if (gpio & maple0) {
                    *data |= mask;
                }
                else {
                    *data &= ~mask;
                }
            }
            crc ^= *data;
            ++data;
        }
maple_end:
        DPORT_STALL_OTHER_CPU_END();

        port = pin_to_port[(__builtin_ffs(maple0) - 1)];
        bad_frame = ((bit_cnt - 1) % 8);

#ifdef WIRED_TRACE
        ets_printf("%08X ", xthal_get_ccount());
        byte = ((bit_cnt - 1) / 8);
        if (bad_frame) {
            ++byte;
            for (uint32_t i = 0; i < byte; ++i) {
                ets_printf("%02X", maple_fix_byte(bad_frame, pkt.data[i ? i - 1 : 0], pkt.data[i]));
            }
        }
        else {
            for (uint32_t i = 0; i < byte; ++i) {
                ets_printf("%02X", pkt.data[i]);
            }
        }
        ets_printf("\n");
#else
        len = ((bit_cnt - 1) / 32) - 1;
        /* Fix up to 7 bits loss */
        if (bad_frame) {
            cmd = maple_fix_byte(bad_frame, pkt.data[2], pkt.data[3]);
            src = maple_fix_byte(bad_frame, pkt.data[1], pkt.data[2]);
            dst = maple_fix_byte(bad_frame, pkt.data[0], pkt.data[1]);
        }
        /* Fix 8 bits loss */
        else if (pkt.len != len) {
            cmd = pkt.data[2];
            src = pkt.data[1];
            dst = pkt.data[0];
            bad_frame = 1;
        }
        else {
            cmd = pkt.cmd;
            src = pkt.dst;
            dst = pkt.src;
        }
        switch(src & ADDR_MASK) {
            case ADDR_CTRL:
                pkt.src = src;
                if (config.out_cfg[port].acc_mode & ACC_RUMBLE) {
                    pkt.src |= ADDR_RUMBLE;
                }
                pkt.dst = dst;
                switch (cmd) {
                    case CMD_INFO_REQ:
                        pkt.len = 28;
                        pkt.cmd = CMD_INFO_RSP;
                        pkt.data32[0] = ID_CTRL;
                        pkt.data32[1] = DESC_CTRL_ALT;
                        pkt.data32[2] = 0;
                        pkt.data32[3] = 0;
                        memcpy((void *)&pkt.data32[4], ctrl_area_dir_name, sizeof(ctrl_area_dir_name));
                        memcpy((void *)&pkt.data32[12], brand, sizeof(brand));
                        pkt.data32[27] = 0xF401;
                        pkt.data32[27] |= 0xAE01 << 16;
                        maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                        ets_printf("%02X %02X: cmd: 0x%02X, %02X\n", src, dst, cmd, port);
                        break;
                    case CMD_GET_CONDITION:
                        pkt.len = 3;
                        pkt.cmd = CMD_DATA_TX;
                        pkt.data32[0] = ID_CTRL;
                        //memcpy((void *)&pkt.data32[1], wired_adapter.data[port].output, sizeof(uint32_t) * 2);
                        pkt.data32[1] = dummy_data[0]++;
                        pkt.data32[2] = dummy_data[1]++;
                        maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                        ++wired_adapter.data[port].frame_cnt;
                        ets_printf("%02X %02X: cmd: 0x%02X, %02X\n", src, dst, cmd, port);
                        break;
                    case CMD_SEND_SERIAL_STRING:
                        ets_printf("%02X %02X: cmd: 0x%02X, %02X, %s\n", src, dst, cmd, port, &pkt.data[4]);
        uint8_t data[545];
                        pkt.len = 1;
                        pkt.cmd = CMD_DATA_TX;
                        pkt.data32[0] = ID_CTRL;
                        maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                        ++wired_adapter.data[port].frame_cnt;
                        break;
                    default:
                        ets_printf("%02X %02X: Unk cmd: 0x%02X, %02X\n", src, dst, cmd, port);
                        break;
                }
                break;
            case ADDR_RUMBLE:
                pkt.src = src;
                pkt.dst = dst;
                switch (cmd) {
                    case CMD_INFO_REQ:
                        pkt.len = 28;
                        pkt.cmd = CMD_INFO_RSP;
                        pkt.data32[0] = ID_RUMBLE;
                        pkt.data32[1] = DESC_RUMBLE;
                        pkt.data32[2] = 0;
                        pkt.data32[3] = 0;
                        memcpy((void *)&pkt.data32[4], rumble_area_dir_name, sizeof(rumble_area_dir_name));
                        memcpy((void *)&pkt.data32[12], brand, sizeof(brand));
                        pkt.data32[27] = 0x4006;
                        pkt.data32[27] |= 0xC800 << 16;
                        maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                        break;
                    case CMD_GET_CONDITION:
                    case CMD_MEM_INFO_REQ:
                        pkt.len = 0x02;
                        pkt.cmd = CMD_DATA_TX;
                        pkt.data32[0] = ID_RUMBLE;
                        pkt.data32[1] = rumble_val;
                        maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                        break;
                    case CMD_BLOCK_READ:
                        pkt.len = 0x03;
                        pkt.cmd = CMD_DATA_TX;
                        pkt.data32[0] = ID_RUMBLE;
                        pkt.data32[1] = 0;
                        pkt.data32[2] = rumble_max;
                        maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                        break;
                    case CMD_BLOCK_WRITE:
                        pkt.len = 0x00;
                        pkt.cmd = CMD_ACK;
                        maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                        if (!bad_frame) {
                            rumble_max = pkt.data32[2];
                        }
                        break;
                    case CMD_SET_CONDITION:
                        pkt.len = 0x00;
                        pkt.cmd = CMD_ACK;
                        maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                        if (!bad_frame) {
                            rumble_val = pkt.data32[1];
                            if (config.out_cfg[port].acc_mode & ACC_RUMBLE) {
                                pkt.data[3] = port;
                                *(uint32_t *)&pkt.data[4] = rumble_max;
                                adapter_q_fb(pkt.data + 3, 9);
                            }
                        }
                        break;
                    default:
                        ets_printf("%02X: Unk cmd: 0x%02X\n", dst, cmd);
                        break;
                }
                break;
        }
#endif

        GPIO.status_w1tc = maple0;
    }
}

void maple_init(void)
{
    printf("Maple Init Started\n");

    gpio_config_t io_conf[4][2] = {0};

    for (uint32_t i = 0; i < ARRAY_SIZE(io_conf); i++) {
        for (uint32_t j = 0; j < ARRAY_SIZE(io_conf[0]); j++) {
            io_conf[i][j].intr_type = j ? 0 : GPIO_PIN_INTR_NEGEDGE;
            io_conf[i][j].pin_bit_mask = BIT(gpio_pin[i][j]);
            io_conf[i][j].mode = GPIO_MODE_INPUT;
            io_conf[i][j].pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf[i][j].pull_up_en = GPIO_PULLUP_DISABLE;
            gpio_config(&io_conf[i][j]);
        }
    }

#if 0
    gpio_config_t io_conf2 = {
        .intr_type = 0,
        .pin_bit_mask = DEBUG,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&io_conf2);
    GPIO.out_w1ts = DEBUG;
#endif

    printf("Maple Init: Enabling interrupt\n");
    esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3, maple_rx, NULL, NULL);
    printf("Maple Init Complete\n");
}
