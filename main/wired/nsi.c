/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <hal/clk_gate_ll.h>
#include <driver/rmt.h>
#include <hal/rmt_ll.h>
#include "zephyr/atomic.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "system/gpio.h"
#include "system/intr.h"
#include "nsi.h"

#define BIT_ZERO 0x80020006
#define BIT_ONE  0x80060002
#define BIT_ONE_MASK 0x7FFC0000
#define STOP_BIT_1US 0x80000002
#define STOP_BIT_2US 0x80000004

#define N64_BIT_PERIOD_TICKS 8
#define GC_BIT_PERIOD_TICKS 10

#define N64_MOUSE 0x0002
#define N64_CTRL 0x0005
#define N64_KB 0x0200
#define N64_SLOT_EMPTY 0x00
#define N64_SLOT_OCCUPIED 0x01

enum {
    RMT_MEM_CHANGE
};

static const uint8_t gpio_pin[4] = {
    19, 5, 26, 27
};

static const uint8_t rmt_ch[4][2] = {
    {0, 0},
    {1, 2},
    {2, 4},
    {3, 6},
};

static const uint8_t rumble_ident[32] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
};
//static const uint8_t transfer_ident[32] = {
//    0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
//    0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
//};
static const uint8_t empty[32] = {0};
static const uint8_t nsi_crc_table[256] = {
    0x8F, 0x85, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0xC2, 0x61, 0xF2, 0x79, 0xFE, 0x7F,
    0xFD, 0xBC, 0x5E, 0x2F, 0xD5, 0xA8, 0x54, 0x2A, 0x15, 0xC8, 0x64, 0x32, 0x19, 0xCE, 0x67, 0xF1,
    0xBA, 0x5D, 0xEC, 0x76, 0x3B, 0xDF, 0xAD, 0x94, 0x4A, 0x25, 0xD0, 0x68, 0x34, 0x1A, 0x0D, 0xC4,
    0x62, 0x31, 0xDA, 0x6D, 0xF4, 0x7A, 0x3D, 0xDC, 0x6E, 0x37, 0xD9, 0xAE, 0x57, 0xE9, 0xB6, 0x5B,
    0xEF, 0xB5, 0x98, 0x4C, 0x26, 0x13, 0xCB, 0xA7, 0x91, 0x8A, 0x45, 0xE0, 0x70, 0x38, 0x1C, 0x0E,
    0x07, 0xC1, 0xA2, 0x51, 0xEA, 0x75, 0xF8, 0x7C, 0x3E, 0x1F, 0xCD, 0xA4, 0x52, 0x29, 0xD6, 0x6B,
    0xF7, 0xB9, 0x9E, 0x4F, 0xE5, 0xB0, 0x58, 0x2C, 0x16, 0x0B, 0xC7, 0xA1, 0x92, 0x49, 0xE6, 0x73,
    0xFB, 0xBF, 0x9D, 0x8C, 0x46, 0x23, 0xD3, 0xAB, 0x97, 0x89, 0x86, 0x43, 0xE3, 0xB3, 0x9B, 0x8F,
    0x85, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0xC2, 0x61, 0xF2, 0x79, 0xFE, 0x7F, 0xFD,
    0xBC, 0x5E, 0x2F, 0xD5, 0xA8, 0x54, 0x2A, 0x15, 0xC8, 0x64, 0x32, 0x19, 0xCE, 0x67, 0xF1, 0xBA,
    0x5D, 0xEC, 0x76, 0x3B, 0xDF, 0xAD, 0x94, 0x4A, 0x25, 0xD0, 0x68, 0x34, 0x1A, 0x0D, 0xC4, 0x62,
    0x31, 0xDA, 0x6D, 0xF4, 0x7A, 0x3D, 0xDC, 0x6E, 0x37, 0xD9, 0xAE, 0x57, 0xE9, 0xB6, 0x5B, 0xEF,
    0xB5, 0x98, 0x4C, 0x26, 0x13, 0xCB, 0xA7, 0x91, 0x8A, 0x45, 0xE0, 0x70, 0x38, 0x1C, 0x0E, 0x07,
    0xC1, 0xA2, 0x51, 0xEA, 0x75, 0xF8, 0x7C, 0x3E, 0x1F, 0xCD, 0xA4, 0x52, 0x29, 0xD6, 0x6B, 0xF7,
    0xB9, 0x9E, 0x4F, 0xE5, 0xB0, 0x58, 0x2C, 0x16, 0x0B, 0xC7, 0xA1, 0x92, 0x49, 0xE6, 0x73, 0xFB,
    0xBF, 0x9D, 0x8C, 0x46, 0x23, 0xD3, 0xAB, 0x97, 0x89, 0x86, 0x43, 0xE3, 0xB3, 0x9B, 0x8F, 0x85,
};

static const uint8_t gc_ident[3] = {0x09, 0x00, 0x20};
static const uint8_t gc_kb_ident[3] = {0x08, 0x20, 0x00};
static const uint8_t gc_neutral[] = {
    0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x20, 0x20, 0x00, 0x00
};
static volatile rmt_item32_t *rmt_items = RMTMEM.chan[0].data32;

//uint8_t mempak[32 * 1024] = {0};

static atomic_t rmt_flags = 0;
static uint8_t buf[128] = {0};
static uint32_t poll_after_mem_wr = 0;
static uint8_t last_rumble[4] = {0};

static inline void load_mouse_axes(uint8_t port, uint8_t *axes) {
    uint8_t *relative = (uint8_t *)(wired_adapter.data[port].output + 2);
    int32_t *raw_axes = (int32_t *)(wired_adapter.data[port].output + 4);
    int32_t val = 0;

    for (uint32_t i = 0; i < 2; i++) {
        if (relative[i]) {
            val = atomic_clear(&raw_axes[i]);
        }
        else {
            val = raw_axes[i];
        }

        if (val > 127) {
            axes[i] = 127;
        }
        else if (val < -128) {
            axes[i] = -128;
        }
        else {
            axes[i] = (uint8_t)val;
        }
    }
}

static uint16_t nsi_bytes_to_items_crc(uint32_t item, const uint8_t *data, uint32_t len, uint8_t *crc, uint32_t stop_bit) {
    const uint8_t *crc_table = nsi_crc_table;
    uint32_t bit_len = item + len * 8;
    volatile uint32_t *item_ptr = &rmt_items[item].val;

    *crc = 0xFF;
    for (; item < bit_len; ++data) {
        for (uint32_t mask = 0x80; mask; mask >>= 1) {
            if (*data & mask) {
                *crc ^= *crc_table;
                *item_ptr = BIT_ONE;
            }
            else {
                *item_ptr = BIT_ZERO;
            }
            ++crc_table;
            ++item_ptr;
            ++item;
        }
    }
    *item_ptr = stop_bit;
    return item;
}

static uint16_t nsi_bytes_to_items_xor(uint32_t item, const uint8_t *data, uint32_t len, uint8_t *xor, uint32_t stop_bit) {
    uint32_t bit_len = item + len * 8;
    volatile uint32_t *item_ptr = &rmt_items[item].val;

    *xor = 0x00;
    for (; item < bit_len; ++data) {
        for (uint32_t mask = 0x80; mask; mask >>= 1) {
            if (*data & mask) {
                *item_ptr = BIT_ONE;
            }
            else {
                *item_ptr = BIT_ZERO;
            }
            ++item_ptr;
            ++item;
        }
        *xor ^= *data;
    }
    *item_ptr = stop_bit;
    return item;
}

static uint16_t nsi_items_to_bytes(uint32_t item, uint8_t *data, uint32_t len) {
    uint32_t bit_len = item + len * 8;
    volatile uint32_t *item_ptr = &rmt_items[item].val;

    for (; item < bit_len; ++data) {
        do {
            if (*item_ptr & BIT_ONE_MASK) {
                *data = (*data << 1) + 1;
            }
            else {
                *data <<= 1;
            }
            ++item_ptr;
            ++item;
        } while ((item % 8));
    }
    return item;
}

static uint16_t nsi_items_to_bytes_crc(uint32_t item, uint8_t *data, uint32_t len, uint8_t *crc) {
    const uint8_t *crc_table = nsi_crc_table;
    uint32_t bit_len = item + len * 8;
    volatile uint32_t *item_ptr = &rmt_items[item].val;

    *crc = 0xFF;
    for (; item < bit_len; ++data) {
        do {
            if (*item_ptr & BIT_ONE_MASK) {
                *crc ^= *crc_table;
            }
            ++crc_table;
            ++item_ptr;
            ++item;
        } while ((item % 8));
    }
    return item;
}

static uint32_t n64_isr(uint32_t cause) {
    const uint32_t intr_st = RMT.int_st.val;
    uint32_t status = intr_st;
    uint16_t item;
    uint8_t i, channel, crc;

    while (status) {
        i = __builtin_ffs(status) - 1;
        status &= ~(1 << i);
        channel = i / 3;
        switch (i % 3) {
            /* TX End */
            case 0:
                //ets_printf("TX_END\n");
                RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
                RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
                /* Go RX right away */
                RMT.conf_ch[channel].conf1.mem_owner = RMT_MEM_OWNER_RX;
                RMT.conf_ch[channel].conf1.rx_en = 1;
                break;
            /* RX End */
            case 1:
                //ets_printf("RX_END\n");
                RMT.conf_ch[channel].conf1.rx_en = 0;
                RMT.conf_ch[channel].conf1.mem_owner = RMT_MEM_OWNER_TX;
                RMT.conf_ch[channel].conf1.mem_wr_rst = 1;
                item = nsi_items_to_bytes(channel * RMT_MEM_ITEM_NUM, buf, 1);
                switch (config.out_cfg[channel].dev_mode) {
                    case DEV_KB:
                        switch (buf[0]) {
                            case 0x00:
                            case 0xFF:
                                *(uint16_t *)buf = N64_KB;
                                buf[2] = N64_SLOT_EMPTY;

                                nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, buf, 3, &crc, STOP_BIT_2US);
                                RMT.conf_ch[channel].conf1.tx_start = 1;
                                break;
                            case 0x13:
                                memcpy(buf, wired_adapter.data[channel].output, 7);
                                nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, buf, 7, &crc, STOP_BIT_2US);
                                RMT.conf_ch[channel].conf1.tx_start = 1;
                                ++wired_adapter.data[channel].frame_cnt;
                                break;
                            default:
                                /* Bad frame go back RX */
                                RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
                                RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
                                RMT.conf_ch[channel].conf1.mem_owner = RMT_MEM_OWNER_RX;
                                RMT.conf_ch[channel].conf1.rx_en = 1;
                                break;
                        }
                        break;
                    case DEV_MOUSE:
                        switch (buf[0]) {
                            case 0x00:
                            case 0xFF:
                                *(uint16_t *)buf = N64_MOUSE;
                                buf[2] = N64_SLOT_EMPTY;

                                nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, buf, 3, &crc, STOP_BIT_2US);
                                RMT.conf_ch[channel].conf1.tx_start = 1;
                                break;
                            case 0x01:
                                    memcpy(buf, wired_adapter.data[channel].output, 2);
                                    load_mouse_axes(channel, &buf[2]);
                                    nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, buf, 4, &crc, STOP_BIT_2US);
                                    RMT.conf_ch[channel].conf1.tx_start = 1;

                                    ++wired_adapter.data[channel].frame_cnt;
                                break;
                            default:
                                /* Bad frame go back RX */
                                RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
                                RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
                                RMT.conf_ch[channel].conf1.mem_owner = RMT_MEM_OWNER_RX;
                                RMT.conf_ch[channel].conf1.rx_en = 1;
                                break;
                        }
                        break;
                    default:
                        switch (buf[0]) {
                            case 0x00:
                            case 0xFF:
                                *(uint16_t *)buf = N64_CTRL;
                                if (config.out_cfg[channel].acc_mode > ACC_NONE) {
                                    buf[2] = N64_SLOT_OCCUPIED;
                                }
                                else {
                                    buf[2] = N64_SLOT_EMPTY;
                                }

                                nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, buf, 3, &crc, STOP_BIT_2US);
                                RMT.conf_ch[channel].conf1.tx_start = 1;
                                break;
                            case 0x01:
                                memcpy(buf, wired_adapter.data[channel].output, 4);
                                nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, buf, 4, &crc, STOP_BIT_2US);
                                RMT.conf_ch[channel].conf1.tx_start = 1;

                                ++wired_adapter.data[channel].frame_cnt;
                                ++poll_after_mem_wr;
                                if (atomic_test_bit(&rmt_flags, RMT_MEM_CHANGE) && poll_after_mem_wr > 3) {
                                    if (!atomic_test_bit(&wired_adapter.data[channel].flags, WIRED_SAVE_MEM)) {
                                        atomic_set_bit(&wired_adapter.data[channel].flags, WIRED_SAVE_MEM);
                                        atomic_clear_bit(&rmt_flags, RMT_MEM_CHANGE);
                                    }
                                }
                                break;
                            case 0x02:
                                item = nsi_items_to_bytes(item, buf, 2);
                                if (buf[0] == 0x80 && buf[1] == 0x01) {
                                    if (config.out_cfg[channel].acc_mode == ACC_RUMBLE) {
                                        item = nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, rumble_ident, 32, &crc, STOP_BIT_2US);
                                    }
                                    else {
                                        item = nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, empty, 32, &crc, STOP_BIT_2US);
                                    }
                                }
                                else {
                                    if (config.out_cfg[channel].acc_mode == ACC_RUMBLE) {
                                        item = nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, empty, 32, &crc, STOP_BIT_2US);
                                    }
                                    else {
                                        //item = nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, mempak + ((buf[0] << 8) | (buf[1] & 0xE0)), 32, &crc, STOP_BIT_2US);
                                    }
                                }
                                buf[0] = crc ^ 0xFF;
                                nsi_bytes_to_items_crc(item, buf, 1, &crc, STOP_BIT_2US);
                                RMT.conf_ch[channel].conf1.tx_start = 1;
                                break;
                            case 0x03:
                                item = nsi_items_to_bytes(item, buf, 2);
                                nsi_items_to_bytes_crc(item, buf + 2, 32, &crc);
                                buf[35] = crc ^ 0xFF;
                                nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, buf + 35, 1, &crc, STOP_BIT_2US);
                                RMT.conf_ch[channel].conf1.tx_start = 1;

                                nsi_items_to_bytes(item, buf + 2, 32);
                                if (config.out_cfg[channel].acc_mode == ACC_RUMBLE) {
                                    if (buf[0] == 0xC0 && last_rumble[channel] != buf[2]) {
                                        last_rumble[channel] = buf[2];
                                        buf[1] = channel;
                                        adapter_q_fb(buf + 1, 2);
                                    }
                                }
                                else {
                                    poll_after_mem_wr = 0;
                                    atomic_set_bit(&rmt_flags, RMT_MEM_CHANGE);
                                    //memcpy(mempak + ((buf[0] << 8) | (buf[1] & 0xE0)),  buf + 2, 32);
                                }
                                break;
                            default:
                                /* Bad frame go back RX */
                                RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
                                RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
                                RMT.conf_ch[channel].conf1.mem_owner = RMT_MEM_OWNER_RX;
                                RMT.conf_ch[channel].conf1.rx_en = 1;
                                break;
                        }
                        break;
                }
                break;
            /* Error */
            case 2:
                ets_printf("ERR\n");
                RMT.int_ena.val &= (~(BIT(i)));
                break;
            default:
                break;
        }
    }
    RMT.int_clr.val = intr_st;
    return 0;
}

static uint32_t gc_isr(uint32_t cause) {
    const uint32_t intr_st = RMT.int_st.val;
    uint32_t status = intr_st;
    uint16_t item;
    uint8_t i, channel, port, crc;

    while (status) {
        i = __builtin_ffs(status) - 1;
        status &= ~(1 << i);
        channel = i / 3;
        port = channel / 2;
        switch (i % 3) {
            /* TX End */
            case 0:
                //ets_printf("TX_END\n");
                RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
                RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
                /* Go RX right away */
                RMT.conf_ch[channel].conf1.mem_owner = RMT_MEM_OWNER_RX;
                RMT.conf_ch[channel].conf1.rx_en = 1;
                break;
            /* RX End */
            case 1:
                //ets_printf("RX_END\n");
                RMT.conf_ch[channel].conf1.rx_en = 0;
                RMT.conf_ch[channel].conf1.mem_owner = RMT_MEM_OWNER_TX;
                RMT.conf_ch[channel].conf1.mem_wr_rst = 1;
                item = nsi_items_to_bytes(channel * RMT_MEM_ITEM_NUM, buf, 1);
                switch (config.out_cfg[port].dev_mode) {
                    case DEV_KB:
                        switch (buf[0]) {
                            case 0x00:
                            case 0xFF:
                                nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, gc_kb_ident, sizeof(gc_kb_ident), &crc, STOP_BIT_2US);
                                RMT.conf_ch[channel].conf1.tx_start = 1;
                                break;
                            case 0x54:
                                item = nsi_bytes_to_items_xor(channel * RMT_MEM_ITEM_NUM, wired_adapter.data[port].output, 7, &crc, STOP_BIT_2US);
                                buf[0] = crc;
                                nsi_bytes_to_items_xor(item, buf, 1, &crc, STOP_BIT_2US);
                                RMT.conf_ch[channel].conf1.tx_start = 1;
                                ++wired_adapter.data[port].frame_cnt;
                                break;
                            default:
                                /* Bad frame go back RX */
                                RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
                                RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
                                RMT.conf_ch[channel].conf1.mem_owner = RMT_MEM_OWNER_RX;
                                RMT.conf_ch[channel].conf1.rx_en = 1;
                                break;
                        }
                        break;
                    default:
                        switch (buf[0]) {
                            case 0x00:
                            case 0xFF:
                                nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, gc_ident, sizeof(gc_ident), &crc, STOP_BIT_2US);
                                RMT.conf_ch[channel].conf1.tx_start = 1;
                                break;
                            case 0x40:
                                nsi_items_to_bytes(item, buf, 2);
                                nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, wired_adapter.data[port].output, 8, &crc, STOP_BIT_2US);
                                RMT.conf_ch[channel].conf1.tx_start = 1;

                                if (config.out_cfg[port].acc_mode == ACC_RUMBLE) {
                                    uint8_t rumble_data[2];
                                    rumble_data[1] = buf[1] & 0x01;
                                    if (last_rumble[port] != rumble_data[1]) {
                                        last_rumble[port] = rumble_data[1];
                                        rumble_data[0] = port;
                                        adapter_q_fb(rumble_data, 2);
                                    }
                                }

                                ++wired_adapter.data[port].frame_cnt;
                                break;
                            case 0x41:
                            case 0x42:
                                nsi_bytes_to_items_crc(channel * RMT_MEM_ITEM_NUM, gc_neutral, sizeof(gc_neutral), &crc, STOP_BIT_2US);
                                RMT.conf_ch[channel].conf1.tx_start = 1;
                                wired_adapter.data[port].output[0] &= ~0x20;
                                break;
                            default:
                                /* Bad frame go back RX */
                                RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
                                RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
                                RMT.conf_ch[channel].conf1.mem_owner = RMT_MEM_OWNER_RX;
                                RMT.conf_ch[channel].conf1.rx_en = 1;
                                break;
                        }
                        break;
                }
                break;
            /* Error */
            case 2:
                ets_printf("ERR\n");
                RMT.int_ena.val &= (~(BIT(i)));
                break;
            default:
                break;
        }
    }
    RMT.int_clr.val = intr_st;
    return 0;
}

void nsi_init(void) {
    uint32_t system = (wired_adapter.system_id == N64) ? 0 : 1;

    periph_ll_enable_clk_clear_rst(PERIPH_RMT_MODULE);

    RMT.apb_conf.fifo_mask = RMT_DATA_MODE_MEM;

    for (uint32_t i = 0; i < ARRAY_SIZE(gpio_pin); i++) {
        RMT.conf_ch[rmt_ch[i][system]].conf0.div_cnt = 40; /* 80MHz (APB CLK) / 40 = 0.5us TICK */;
        RMT.conf_ch[rmt_ch[i][system]].conf1.mem_rd_rst = 1;
        RMT.conf_ch[rmt_ch[i][system]].conf1.mem_wr_rst = 1;
        RMT.conf_ch[rmt_ch[i][system]].conf1.tx_conti_mode = 0;
        RMT.conf_ch[rmt_ch[i][system]].conf0.mem_size = 8;
        RMT.conf_ch[rmt_ch[i][system]].conf1.mem_owner = RMT_MEM_OWNER_TX;
        RMT.conf_ch[rmt_ch[i][system]].conf1.ref_always_on = RMT_BASECLK_APB;
        RMT.conf_ch[rmt_ch[i][system]].conf1.idle_out_en = 1;
        RMT.conf_ch[rmt_ch[i][system]].conf1.idle_out_lv = RMT_IDLE_LEVEL_HIGH;
        RMT.conf_ch[rmt_ch[i][system]].conf0.carrier_en = 0;
        RMT.conf_ch[rmt_ch[i][system]].conf0.carrier_out_lv = RMT_CARRIER_LEVEL_LOW;
        RMT.carrier_duty_ch[rmt_ch[i][system]].high = 0;
        RMT.carrier_duty_ch[rmt_ch[i][system]].low = 0;
        RMT.conf_ch[rmt_ch[i][system]].conf0.idle_thres = (wired_adapter.system_id == N64) ? N64_BIT_PERIOD_TICKS : GC_BIT_PERIOD_TICKS;
        RMT.conf_ch[rmt_ch[i][system]].conf1.rx_filter_thres = 0; /* No minimum length */
        RMT.conf_ch[rmt_ch[i][system]].conf1.rx_filter_en = 0;

        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[gpio_pin[i]], PIN_FUNC_GPIO);
        gpio_set_direction_iram(gpio_pin[i], GPIO_MODE_INPUT_OUTPUT_OD); /* Bidirectional open-drain */
        gpio_matrix_out(gpio_pin[i], RMT_SIG_OUT0_IDX + rmt_ch[i][system], 0, 0);
        gpio_matrix_in(gpio_pin[i], RMT_SIG_IN0_IDX + rmt_ch[i][system], 0);

        rmt_ll_enable_tx_end_interrupt(&RMT, rmt_ch[i][system], 1);
        rmt_ll_enable_rx_end_interrupt(&RMT, rmt_ch[i][system], 1);
        rmt_ll_enable_rx_err_interrupt(&RMT, rmt_ch[i][system], 1);

        rmt_ll_rx_enable(&RMT, rmt_ch[i][system], 0);
        rmt_ll_rx_reset_pointer(&RMT, rmt_ch[i][system]);
        rmt_ll_clear_rx_end_interrupt(&RMT, rmt_ch[i][system]);
        rmt_ll_enable_rx_end_interrupt(&RMT, rmt_ch[i][system], 1);
        rmt_ll_rx_enable(&RMT, rmt_ch[i][system], 1);
    }

    intexc_alloc_iram(ETS_RMT_INTR_SOURCE, 19, wired_adapter.system_id == N64 ? n64_isr : gc_isr);
}

