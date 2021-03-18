/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_timer.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "system/core0_stall.h"
#include "system/delay.h"
#include "system/gpio.h"
#include "system/intr.h"
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

#define ADDR_MASK   0x3F
#define ADDR_MAIN   0x20
#define ADDR_MEM    0x01
#define ADDR_RUMBLE 0x02

#define DESC_CTRL     0x000F06FE
#define DESC_CTRL_ALT 0x003FFFFF
#define DESC_VMU      0x7E7E3F40
#define DESC_RUMBLE   0x01010000
#define DESC_MOUSE    0x000E0700
#define DESC_KB       0x01020080

#define PWR_CTRL   0xAE01F401
#define PWR_VMU    0x7C008200
#define PWR_RUMBLE 0xC8004006
#define PWR_MOUSE  0x9001F401
#define PWR_KB     0x5E019001

#define TIMEOUT 8

#define wait_100ns() asm("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
#define maple_fix_byte(s, a, b) (s ? ((a << s) | (b >> (8 - s))) : b)

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

static const uint8_t gpio_pin[][2] = {
    {21, 22},
#ifndef CONFIG_BLUERETRO_WIRED_TRACE
    { 3,  5},
    {18, 23},
    {26, 27},
#endif
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

#ifndef CONFIG_BLUERETRO_WIRED_TRACE
static const uint8_t ctrl_area_dir_name[] = {
    0x72, 0x44, 0x00, 0xFF, 0x63, 0x6D, 0x61, 0x65, 0x20, 0x74, 0x73, 0x61, 0x74, 0x6E, 0x6F, 0x43,
    0x6C, 0x6C, 0x6F, 0x72, 0x20, 0x20, 0x72, 0x65, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};

#if 0
static const uint8_t vmu_area_dir_name[] = {
    0x69, 0x56, 0x00, 0xFF, 0x6C, 0x61, 0x75, 0x73, 0x6D, 0x65, 0x4D, 0x20, 0x20, 0x79, 0x72, 0x6F,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};
#endif

static const uint8_t rumble_area_dir_name[] = {
    0x75, 0x50, 0x00, 0xFF, 0x50, 0x20, 0x75, 0x72, 0x20, 0x75, 0x72, 0x75, 0x6B, 0x63, 0x61, 0x50,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};

static const uint8_t mouse_area_dir_name[] = {
    0x72, 0x44, 0x00, 0xFF, 0x63, 0x6D, 0x61, 0x65, 0x20, 0x74, 0x73, 0x61, 0x73, 0x75, 0x6F, 0x4D,
    0x20, 0x20, 0x20, 0x65, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};

static const uint8_t kb_area_dir_name[] = {
    0x65, 0x4B, 0x00, 0x02, 0x61, 0x6F, 0x62, 0x79, 0x20, 0x20, 0x64, 0x72, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};

static const uint8_t brand[] = {
    0x64, 0x6F, 0x72, 0x50, 0x64, 0x65, 0x63, 0x75, 0x20, 0x79, 0x42, 0x20, 0x55, 0x20, 0x72, 0x6F,
    0x72, 0x65, 0x64, 0x6E, 0x63, 0x69, 0x4C, 0x20, 0x65, 0x73, 0x6E, 0x65, 0x6F, 0x72, 0x46, 0x20,
    0x45, 0x53, 0x20, 0x6D, 0x45, 0x20, 0x41, 0x47, 0x52, 0x45, 0x54, 0x4E, 0x53, 0x49, 0x52, 0x50,
    0x4C, 0x2C, 0x53, 0x45, 0x20, 0x2E, 0x44, 0x54, 0x20, 0x20, 0x20, 0x20,
};

static const uint8_t dc_mouse_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    2,       3,       1,       0,       5,      4
};

#else
static uint32_t cur_us = 0, pre_us = 0;
#endif

static struct maple_pkt pkt;
static uint32_t rumble_max = 0x00020013;
static uint32_t rumble_val = 0x10E0073B;

static inline void load_mouse_axes(uint8_t port, uint16_t *axes) {
    uint8_t *relative = (uint8_t *)(wired_adapter.data[port].output + 4);
    int32_t *raw_axes = (int32_t *)(wired_adapter.data[port].output + 8);
    int32_t val = 0;

    for (uint32_t i = 0; i < 4; i++) {
        if (relative[i]) {
            val = atomic_clear(&raw_axes[i]);
        }
        else {
            val = raw_axes[i];
        }

        if (val > 511) {
            axes[dc_mouse_axes_idx[i]] = sys_cpu_to_be16(0x3FF);
        }
        else if (val < -512) {
            axes[dc_mouse_axes_idx[i]] = sys_cpu_to_be16(0);
        }
        else {
            axes[dc_mouse_axes_idx[i]] = sys_cpu_to_be16((uint16_t)(val + 0x200));
        }
    }
}

static void maple_tx(uint32_t port, uint32_t maple0, uint32_t maple1, uint8_t *data, uint8_t len) {
    uint8_t *crc = data + (len - 1);
    *crc = 0x00;

    delay_us(55);

    GPIO.out_w1ts = maple0 | maple1;
    gpio_set_direction_iram(gpio_pin[port][0], GPIO_MODE_OUTPUT);
    gpio_set_direction_iram(gpio_pin[port][1], GPIO_MODE_OUTPUT);
    core0_stall_start();
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

    core0_stall_end();
    gpio_set_direction_iram(gpio_pin[port][0], GPIO_MODE_INPUT);
    gpio_set_direction_iram(gpio_pin[port][1], GPIO_MODE_INPUT);
}

static uint32_t maple_rx(uint32_t cause) {
    const uint32_t maple0 = GPIO.acpu_int;
    uint32_t timeout;
    uint32_t bit_cnt = 0;
    uint32_t gpio;
    uint8_t *data = pkt.data;
#ifdef CONFIG_BLUERETRO_WIRED_TRACE
    uint32_t byte;
#endif
    uint32_t port;
    uint32_t bad_frame;
    uint8_t len, cmd, src, dst, crc = 0;
    uint32_t maple1;

    if (maple0) {
        core0_stall_start();
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
        core0_stall_end();
        port = pin_to_port[(__builtin_ffs(maple0) - 1)];
        bad_frame = ((bit_cnt - 1) % 8);

#ifdef CONFIG_BLUERETRO_WIRED_TRACE
        pre_us = cur_us;
        cur_us = xthal_get_ccount();;
        ets_printf("+%07u: ", (cur_us - pre_us)/CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ);
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
        switch (config.out_cfg[port].dev_mode) {
            case DEV_KB:
                switch(src & ADDR_MASK) {
                    case ADDR_MAIN:
                        pkt.src = src;
                        pkt.dst = dst;
                        switch (cmd) {
                            case CMD_INFO_REQ:
                                pkt.len = 28;
                                pkt.cmd = CMD_INFO_RSP;
                                pkt.data32[0] = ID_KB;
                                pkt.data32[1] = DESC_KB;
                                pkt.data32[2] = 0;
                                pkt.data32[3] = 0;
                                memcpy((void *)&pkt.data32[4], kb_area_dir_name, sizeof(kb_area_dir_name));
                                memcpy((void *)&pkt.data32[12], brand, sizeof(brand));
                                pkt.data32[27] = PWR_KB;
                                maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                                break;
                            case CMD_GET_CONDITION:
                                pkt.len = 3;
                                pkt.cmd = CMD_DATA_TX;
                                pkt.data32[0] = ID_KB;
                                memcpy((void *)&pkt.data32[1], wired_adapter.data[port].output, sizeof(uint32_t) * 2);
                                maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                                ++wired_adapter.data[port].frame_cnt;
                                break;
                            default:
                                ets_printf("%02X: Unk cmd: 0x%02X\n", dst, cmd);
                                break;
                        }
                        break;
                }
                break;
            case DEV_MOUSE:
                switch(src & ADDR_MASK) {
                    case ADDR_MAIN:
                        pkt.src = src;
                        pkt.dst = dst;
                        switch (cmd) {
                            case CMD_INFO_REQ:
                                pkt.len = 28;
                                pkt.cmd = CMD_INFO_RSP;
                                pkt.data32[0] = ID_MOUSE;
                                pkt.data32[1] = DESC_MOUSE;
                                pkt.data32[2] = 0;
                                pkt.data32[3] = 0;
                                memcpy((void *)&pkt.data32[4], mouse_area_dir_name, sizeof(mouse_area_dir_name));
                                memcpy((void *)&pkt.data32[12], brand, sizeof(brand));
                                pkt.data32[27] = PWR_MOUSE;
                                maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                                break;
                            case CMD_GET_CONDITION:
                                pkt.len = 6;
                                pkt.cmd = CMD_DATA_TX;
                                pkt.data32[0] = ID_MOUSE;
                                memcpy((void *)&pkt.data32[1], wired_adapter.data[port].output, sizeof(uint32_t) * 1);
                                load_mouse_axes(port, (uint16_t *)&pkt.data32[2]);
                                maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                                ++wired_adapter.data[port].frame_cnt;
                                break;
                            default:
                                ets_printf("%02X: Unk cmd: 0x%02X\n", dst, cmd);
                                break;
                        }
                        break;
                }
                break;
            default:
                switch(src & ADDR_MASK) {
                    case ADDR_MAIN:
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
                                if (config.out_cfg[port].dev_mode == DEV_PAD_ALT) {
                                    pkt.data32[1] = DESC_CTRL_ALT;
                                }
                                else {
                                    pkt.data32[1] = DESC_CTRL;
                                }
                                pkt.data32[2] = 0;
                                pkt.data32[3] = 0;
                                memcpy((void *)&pkt.data32[4], ctrl_area_dir_name, sizeof(ctrl_area_dir_name));
                                memcpy((void *)&pkt.data32[12], brand, sizeof(brand));
                                pkt.data32[27] = PWR_CTRL;
                                maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                                break;
                            case CMD_GET_CONDITION:
                                pkt.len = 3;
                                pkt.cmd = CMD_DATA_TX;
                                pkt.data32[0] = ID_CTRL;
                                memcpy((void *)&pkt.data32[1], wired_adapter.data[port].output, sizeof(uint32_t) * 2);
                                maple_tx(port, maple0, maple1, pkt.data, pkt.len * 4 + 5);
                                ++wired_adapter.data[port].frame_cnt;
                                break;
                            default:
                                ets_printf("%02X: Unk cmd: 0x%02X\n", dst, cmd);
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
                                pkt.data32[27] = PWR_RUMBLE;
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
                break;
        }
#endif

        GPIO.status_w1tc = maple0;
    }
    return 0;
}

void maple_init(void)
{
    gpio_config_t io_conf;

    for (uint32_t i = 0; i < ARRAY_SIZE(gpio_pin); i++) {
        for (uint32_t j = 0; j < ARRAY_SIZE(gpio_pin[0]); j++) {
            io_conf.intr_type = j ? 0 : GPIO_INTR_NEGEDGE;
            io_conf.pin_bit_mask = BIT(gpio_pin[i][j]);
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            gpio_config_iram(&io_conf);
        }
    }

    intexc_alloc_iram(ETS_GPIO_INTR_SOURCE, 19, maple_rx);
}
