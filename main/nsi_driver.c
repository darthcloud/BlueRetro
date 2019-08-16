/* ESP32 RMT NSI Driver
 *
 * ESP32 app demonstrating sniffing Nintendo's serial interface via RMT peripheral.
 *
*/
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/rmt.h>
#include <esp_task_wdt.h>
#include "nsi.h"
#include "zephyr/atomic.h"

#define BIT_ZERO 0x80020006
#define BIT_ONE  0x80060002
#define BIT_ONE_MASK 0x7FFC0000
#define STOP_BIT_1US 0x80000002
#define STOP_BIT_2US 0x80000004

#define NSI_BIT_PERIOD_TICKS 8

enum {
    RMT_MEM_CHANGE
};
static const uint8_t rumble_ident[32] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};
static const uint8_t transfer_ident[32] = {
    0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
    0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84
};
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
    0xBF, 0x9D, 0x8C, 0x46, 0x23, 0xD3, 0xAB, 0x97, 0x89, 0x86, 0x43, 0xE3, 0xB3, 0x9B, 0x8F, 0x85
};

typedef struct {
    nsi_mode_t nsi_mode;
    TaskHandle_t *nsi_task_handle;
    rmt_isr_handle_t nsi_isr_handle;
} nsi_channel_handle_t;

static nsi_channel_handle_t nsi[NSI_CH_MAX] = {{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}};
static volatile rmt_item32_t *rmt_items = RMTMEM.chan[0].data32;

uint8_t mempak[32 * 1024] = {0};

atomic_t rmt_flags = 0;

static struct io *output;
static uint8_t buf[32 * 1024] = {0};
static uint8_t ctrl_ident[3] = {0x05, 0x00, 0x01};
static uint32_t poll_after_mem_wr = 0;

static uint16_t IRAM_ATTR nsi_bytes_to_items_crc(nsi_channel_t channel, uint32_t ch_offset, const uint8_t *data, uint32_t len, uint8_t *crc, uint32_t stop_bit) {
    uint32_t item = (channel * RMT_MEM_ITEM_NUM + ch_offset);
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

static uint16_t IRAM_ATTR nsi_items_to_bytes(nsi_channel_t channel, uint32_t ch_offset, uint8_t *data, uint32_t len) {
    uint32_t item = (channel * RMT_MEM_ITEM_NUM + ch_offset);
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

static uint16_t IRAM_ATTR nsi_items_to_bytes_crc(nsi_channel_t channel, uint32_t ch_offset, uint8_t *data, uint32_t len, uint8_t *crc) {
    uint32_t item = (channel * RMT_MEM_ITEM_NUM + ch_offset);
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

static void IRAM_ATTR nsi_isr(void *arg) {
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
                item = nsi_items_to_bytes(channel, 0, buf, 1);
                switch (buf[0]) {
                    case 0x00:
                    case 0xFF:
                        output->format = IO_FORMAT_N64;

                        if (output->mode) {
                            ctrl_ident[2] = 0x01;
                        }
                        else {
                            ctrl_ident[2] = 0x00;
                        }
                        nsi_bytes_to_items_crc(channel, 0, ctrl_ident, 3, &crc, STOP_BIT_2US);
                        RMT.conf_ch[channel].conf1.tx_start = 1;
                        break;
                    case 0x01:
                        poll_after_mem_wr++;
                        if (atomic_test_bit(&rmt_flags, RMT_MEM_CHANGE) && poll_after_mem_wr > 3) {
                            if (!atomic_test_bit(&output->flags, WRIO_SAVE_MEM)) {
                                atomic_set_bit(&output->flags, WRIO_SAVE_MEM);
                                atomic_clear_bit(&rmt_flags, RMT_MEM_CHANGE);
                            }
                        }

                        nsi_bytes_to_items_crc(channel, 0, (uint8_t *)&output->io.n64, 4, &crc, STOP_BIT_2US);
                        RMT.conf_ch[channel].conf1.tx_start = 1;
                        break;
                    case 0x02:
                        item = nsi_items_to_bytes(channel, item, buf, 2);
                        if (buf[0] == 0x80 && buf[1] == 0x01) {
                            if (output->mode == 0x01) {
                                item = nsi_bytes_to_items_crc(channel, 0, rumble_ident, 32, &crc, STOP_BIT_2US);
                            }
                            else {
                                item = nsi_bytes_to_items_crc(channel, 0, empty, 32, &crc, STOP_BIT_2US);
                            }
                        }
                        else {
                            if (output->mode == 0x01) {
                                item = nsi_bytes_to_items_crc(channel, 0, empty, 32, &crc, STOP_BIT_2US);
                            }
                            else {
                                item = nsi_bytes_to_items_crc(channel, 0, mempak + ((buf[0] << 8) | (buf[1] & 0xE0)), 32, &crc, STOP_BIT_2US);
                            }
                        }
                        buf[0] = crc ^ 0xFF;
                        nsi_bytes_to_items_crc(channel, item, buf, 1, &crc, STOP_BIT_2US);
                        RMT.conf_ch[channel].conf1.tx_start = 1;
                        break;
                    case 0x03:
                        item = nsi_items_to_bytes(channel, item, buf, 2);
                        nsi_items_to_bytes_crc(channel, item, buf + 2, 32, &crc);
                        buf[35] = crc ^ 0xFF;
                        nsi_bytes_to_items_crc(channel, 0, buf + 35, 1, &crc, STOP_BIT_2US);
                        RMT.conf_ch[channel].conf1.tx_start = 1;
                        nsi_items_to_bytes(channel, item, buf + 2, 32);
                        if (output->mode == 0x01) {
                            if (buf[0] == 0xC0) {
                                if (buf[2] & 0x01) {
                                    atomic_set_bit(&output->flags, WRIO_RUMBLE_ON);
                                }
                                else {
                                    atomic_clear_bit(&output->flags, WRIO_RUMBLE_ON);
                                }
                            }
                        }
                        else {
                            poll_after_mem_wr = 0;
                            atomic_set_bit(&rmt_flags, RMT_MEM_CHANGE);
                            memcpy(mempak + ((buf[0] << 8) | (buf[1] & 0xE0)),  buf + 2, 32);
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
}

void nsi_init(nsi_channel_t channel, uint8_t gpio, nsi_mode_t mode, struct io *output_data) {
    nsi[channel].nsi_mode = mode;
    output = output_data;

    periph_module_enable(PERIPH_RMT_MODULE);

    RMT.apb_conf.fifo_mask = RMT_DATA_MODE_MEM;

    RMT.conf_ch[channel].conf0.div_cnt = 40; /* 80MHz (APB CLK) / 40 = 0.5us TICK */;
    RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
    RMT.conf_ch[channel].conf1.mem_wr_rst = 1;
    RMT.conf_ch[channel].conf1.tx_conti_mode = 0;
    RMT.conf_ch[channel].conf0.mem_size = 8;
    RMT.conf_ch[channel].conf1.mem_owner = RMT_MEM_OWNER_TX;
    RMT.conf_ch[channel].conf1.ref_always_on = RMT_BASECLK_APB;
    RMT.conf_ch[channel].conf1.idle_out_en = 1;
    RMT.conf_ch[channel].conf1.idle_out_lv = RMT_IDLE_LEVEL_HIGH;
    RMT.conf_ch[channel].conf0.carrier_en = 0;
    RMT.conf_ch[channel].conf0.carrier_out_lv = RMT_CARRIER_LEVEL_LOW;
    RMT.carrier_duty_ch[channel].high = 0;
    RMT.carrier_duty_ch[channel].low = 0;
    RMT.conf_ch[channel].conf0.idle_thres = NSI_BIT_PERIOD_TICKS;
    RMT.conf_ch[channel].conf1.rx_filter_thres = 0; /* No minimum length */
    RMT.conf_ch[channel].conf1.rx_filter_en = 0;

    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
    gpio_set_direction(gpio, GPIO_MODE_INPUT_OUTPUT_OD); /* Bidirectional open-drain */
    gpio_matrix_out(gpio, RMT_SIG_OUT0_IDX + channel, 0, 0);
    gpio_matrix_in(gpio, RMT_SIG_IN0_IDX + channel, 0);

    rmt_set_tx_intr_en(channel, 1);
    rmt_set_rx_intr_en(channel, 1);
    rmt_set_err_intr_en(channel, 1);
    rmt_isr_register(nsi_isr, NULL,
                     0, &nsi[channel].nsi_isr_handle);

    rmt_rx_start(channel, 1);
}

