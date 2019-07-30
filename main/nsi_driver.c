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

#define BIT_ZERO 0x80020006
#define BIT_ONE  0x80060002
#define STOP_BIT_1US 0x80000002
#define STOP_BIT_2US 0x80000004

#define NSI_BIT_PERIOD_TICKS 8

static struct io *output;
static nsi_frame_t nsi_frame = {0};
static  nsi_frame_t nsi_ident = {{0x05, 0x00, 0x00}, 3, 2};
static  nsi_frame_t nsi_status = {{0x00, 0x00, 0x00, 0x00}, 4, 2};

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

static uint16_t IRAM_ATTR nsi_bytes_to_items(nsi_channel_t channel, nsi_frame_t *frame) {
    uint8_t byte, bit,*val;//, crc = 0xFF;
    uint16_t item, idx;

    //int16_t crc_bit = nsi[channel].nsi_mode ? 0 : -24;

    //printf("len %d stop %d\n", frame->len, frame->stop_len);

    for (idx = item = (channel * RMT_MEM_ITEM_NUM), val = frame->data; item < frame->len; val++) {
        //printf("%02X\n", val);
        //if (byte == 3) {
        //    RMT.conf_ch[channel].conf1.tx_start = 1;
        //}
        //for (; (item % 8); item++,  *val <<= 1/*, crc_bit++*/) {
        do {
            //ets_printf("%02X\n", *val);
            //if (crc_bit == 0)
            //    crc = 0xFF;
            //rmt_items[idx].level0 = 0;
            //rmt_items[idx].level1 = 1;
            if (*val & 0x80) {
                //if (crc_bit < 256)
                //    crc ^= nsi_crc_table[crc_bit];
                rmt_items[item].val = BIT_ONE;
            }
            else {
                rmt_items[item].val = BIT_ZERO;
            }
            item++;
            *val <<= 1;
        } while ((item % 8));
    }
    //rmt_items[idx].level0 = 0;
    //rmt_items[idx].level1 = 1;
    rmt_items[item].val = STOP_BIT_2US;
    //printf("crc: 0x%02X or 0x%02X\n", crc, crc ^ 0xFF);
    return item;
}

static uint8_t IRAM_ATTR nsi_items_to_bytes(nsi_channel_t channel, nsi_frame_t *frame) {
    uint8_t byte, bit, crc = 0xFF;
    uint16_t item, idx;
    int16_t crc_bit = nsi[channel].nsi_mode ? -24: 0;

    for (idx = item = (channel * RMT_MEM_ITEM_NUM), byte = 0; rmt_items[idx].duration1; byte++) {
        for (bit = 0; bit < 8; bit++, item++, idx = (item & 0x1FF), crc_bit++) {
            if (crc_bit == 0)
                crc = 0xFF;
            frame->data[byte] <<= 1;
            if (rmt_items[idx].duration1 > (NSI_BIT_PERIOD_TICKS * 1/2)) {
                if (crc_bit < 256)
                    crc ^= nsi_crc_table[crc_bit];
                frame->data[byte] |= 1;
            }
        }
    }
    frame->stop_len = rmt_items[idx].duration0 / 2;
    //if (frame.crc)
    //    *frame.crc = crc;
    //printf("crc: 0x%02X or 0x%02X\n", crc, crc ^ 0xFF);
    return (frame->len = byte);
}

static void IRAM_ATTR nsi_isr(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    const uint32_t intr_st = RMT.int_st.val;
    uint32_t status = intr_st;
    uint8_t i, channel;

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
                nsi_items_to_bytes(0, &nsi_frame);
                switch (nsi_frame.data[0]) {
                    case 0x00:
                        memcpy(nsi_frame.data, nsi_ident.data, 3);
                        nsi_frame.len = 24;
                        break;
                    case 0x01:
                        memcpy(nsi_frame.data, &output->io.n64, 4);
                        nsi_frame.len = 32;
                        break;
                }
                nsi_frame.stop_len = 2;
                nsi_bytes_to_items(0, &nsi_frame);
                RMT.conf_ch[channel].conf1.tx_start = 1;
                //vTaskNotifyGiveFromISR(*nsi[channel].nsi_task_handle,
                //                        &xHigherPriorityTaskWoken);
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
    if (xHigherPriorityTaskWoken)
        portYIELD_FROM_ISR();
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

    for (uint16_t i = 0; i < 512; i++) {
        rmt_items[i].level0 = 0;
        rmt_items[i].level1 = 1;
    }

    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
    gpio_set_direction(gpio, GPIO_MODE_INPUT_OUTPUT_OD); /* Bidirectional open-drain */
    gpio_matrix_out(gpio, RMT_SIG_OUT0_IDX + channel, 0, 0);
    gpio_matrix_in(gpio, RMT_SIG_IN0_IDX + channel, 0);

    rmt_set_tx_intr_en(channel, 1);
    rmt_set_rx_intr_en(channel, 1);
    rmt_set_err_intr_en(channel, 1);
    rmt_isr_register(nsi_isr, NULL,
                     0, &nsi[channel].nsi_isr_handle);

    /* Workaround: ISR give not RX by task until ~10ms */
    vTaskDelay(10 / portTICK_PERIOD_MS);

    rmt_rx_start(channel, 1);
}

void nsi_reset(nsi_channel_t channel) {
    rmt_isr_deregister(nsi[channel].nsi_isr_handle);
    rmt_set_err_intr_en(channel, 0);
    rmt_set_rx_intr_en(channel, 0);
    rmt_set_tx_intr_en(channel, 0);

    RMT.conf_ch[channel].conf1.mem_owner = RMT_MEM_OWNER_TX;
    RMT.conf_ch[channel].conf1.mem_wr_rst = 1;
    RMT.conf_ch[channel].conf1.mem_rd_rst = 1;

    periph_module_disable(PERIPH_RMT_MODULE);
}

