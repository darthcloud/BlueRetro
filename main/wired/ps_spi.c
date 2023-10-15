/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "soc/io_mux_reg.h"
#include "esp_private/periph_ctrl.h"
#include <soc/spi_periph.h>
#include <esp32/rom/ets_sys.h>
#include <esp32/rom/gpio.h>
#include "hal/clk_gate_ll.h"
#include "driver/gpio.h"
#include "system/intr.h"
#include "system/gpio.h"
#include "system/delay.h"
#include "system/led.h"
#include "zephyr/atomic.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "adapter/kb_monitor.h"
#include "adapter/wired/ps.h"
#include "wired_bare.h"
#include "ps_spi.h"
#include "sdkconfig.h"

enum {
    DEV_NONE = 0,
    DEV_PSX_DIGITAL,
    DEV_PSX_FLIGHT,
    DEV_PSX_ANALOG,
    DEV_PSX_DS1,
    DEV_PSX_MULTITAP,
    DEV_PSX_MOUSE,
    DEV_PSX_PS_2_KB_MOUSE_ADAPTER,
    DEV_PS2_DS2,
    DEV_PS2_MULTITAP,
    DEV_TYPE_MAX,
};

#define ESP_SPI2_HSPI 1
#define ESP_SPI3_VSPI 2

#define SPI2_HSPI_INTR_NUM 19
#define SPI3_VSPI_INTR_NUM 20
#define GPIO_INTR_NUM 21

#define SPI2_HSPI_INTR_MASK (1 << SPI2_HSPI_INTR_NUM)
#define SPI3_VSPI_INTR_MASK (1 << SPI3_VSPI_INTR_NUM)
#define GPIO_INTR_MASK (1 << GPIO_INTR_NUM)

/* Name relative to console */
#define P1_DTR_PIN 34
#define P1_SCK_PIN 33
#define P1_TXD_PIN 32
#define P1_RXD_PIN 19
#define P1_DSR_PIN 21

#define P2_DTR_PIN 5
#define P2_SCK_PIN 26
#define P2_TXD_PIN 27
#define P2_RXD_PIN 22
#define P2_DSR_PIN 25

#define P1_ANALOG_LED_PIN 12
#define P2_ANALOG_LED_PIN 15

#define PS_SPI_DTR 0
#define PS_SPI_SCK 1
#define PS_SPI_TXD 2
#define PS_SPI_RXD 3
#define PS_SPI_DSR 4

#define PS_BUFFER_SIZE 64
#define PS_PORT_MAX 2
#define MT_PORT_MAX 4

struct ps_ctrl_port {
    struct spi_cfg cfg;
    spi_dev_t *spi_hw;
    uint8_t id;
    uint8_t led_pin;
    uint8_t root_dev_type;
    uint8_t mt_state;
    uint8_t mt_first_port;
    uint8_t tx_buf_len;
    uint8_t tx_buf[PS_BUFFER_SIZE];
    uint8_t rx_buf[2][PS_BUFFER_SIZE];
    uint8_t active_rx_buf;
    uint8_t dev_type[MT_PORT_MAX];
    uint8_t dev_id[MT_PORT_MAX];
    uint8_t pend_dev_id[MT_PORT_MAX];
    uint8_t last_rumble[MT_PORT_MAX];
    uint8_t rumble_r_state[MT_PORT_MAX];
    uint8_t rumble_l_state[MT_PORT_MAX];
    uint8_t rumble_r_idx[MT_PORT_MAX];
    uint8_t rumble_l_idx[MT_PORT_MAX];
    uint8_t analog_btn[MT_PORT_MAX];
    uint32_t dev_desc[MT_PORT_MAX];
    uint32_t idx;
    uint32_t mt_idx;
    uint32_t valid;
    uint32_t game_id_len;
};

static struct ps_ctrl_port ps_ctrl_ports[PS_PORT_MAX] = {
    {
        .cfg = {
            .hw = &SPI2,
            .write_bit_order = 1,
            .read_bit_order = 1,
            /* Set Mode 3 as per ESP32 TRM, except ck_i_edge that need to be 1 for original PSX! */
            .clk_idle_edge = 0,
            .clk_i_edge = 1,
            .miso_delay_mode = 1,
            .miso_delay_num = 0,
            .mosi_delay_mode = 0,
            .mosi_delay_num = 0,
            .write_bit_len = 8 - 1,
            .read_bit_len = 8 - 1,
            .inten = 1,
        },
        .id = 0,
        .led_pin = P1_ANALOG_LED_PIN,
        .spi_hw = &SPI2,
    },
    {
        .cfg = {
            .hw = &SPI3,
            .write_bit_order = 1,
            .read_bit_order = 1,
            /* Set Mode 3 as per ESP32 TRM, except ck_i_edge that need to be 1 for original PSX! */
            .clk_idle_edge = 0,
            .clk_i_edge = 1,
            .miso_delay_mode = 1,
            .miso_delay_num = 0,
            .mosi_delay_mode = 0,
            .mosi_delay_num = 0,
            .write_bit_len = 8 - 1,
            .read_bit_len = 8 - 1,
            .inten = 1,
        },
        .id = 1,
        .led_pin = P2_ANALOG_LED_PIN,
        .spi_hw = &SPI3,
    }
};

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

static uint32_t get_dtr_state(uint32_t port) {
    if (port == 0) {
        if (GPIO.in1.val & BIT(P1_DTR_PIN - 32)) {
            return 1;
        }
    }
    else {
        if (GPIO.in & BIT(P2_DTR_PIN)) {
            return 1;
        }
    }
    return 0;
}

static void set_output_state(uint32_t port, uint32_t enable) {
    if (port == 0) {
        if (enable) {
            gpio_set_direction_iram(P1_DSR_PIN, GPIO_MODE_OUTPUT_OD);
            gpio_set_direction_iram(P1_RXD_PIN, GPIO_MODE_OUTPUT_OD);
            gpio_matrix_out(P1_RXD_PIN, HSPIQ_OUT_IDX, false, false);
        }
        else {
            gpio_set_direction_iram(P1_RXD_PIN, GPIO_MODE_INPUT);
            gpio_set_direction_iram(P1_DSR_PIN, GPIO_MODE_INPUT);
        }
    }
    else {
        if (enable) {
            gpio_set_direction_iram(P2_DSR_PIN, GPIO_MODE_OUTPUT_OD);
            gpio_set_direction_iram(P2_RXD_PIN, GPIO_MODE_OUTPUT_OD);
            gpio_matrix_out(P2_RXD_PIN, VSPIQ_OUT_IDX, false, false);
        }
        else {
            gpio_set_direction_iram(P2_RXD_PIN, GPIO_MODE_INPUT);
            gpio_set_direction_iram(P2_DSR_PIN, GPIO_MODE_INPUT);
        }
    }
}

static void toggle_dsr(uint32_t port) {
    if (port == 0) {
        GPIO.out_w1tc = BIT(P1_DSR_PIN);
        delay_us(2);
        GPIO.out_w1ts = BIT(P1_DSR_PIN);
    }
    else {
        GPIO.out_w1tc = BIT(P2_DSR_PIN);
        delay_us(2);
        GPIO.out_w1ts = BIT(P2_DSR_PIN);
    }
}

static void ps_analog_btn_hdlr(struct ps_ctrl_port *port, uint8_t id) {
    if (port->dev_type[id] != DEV_PSX_MOUSE && port->dev_type[id] != DEV_PSX_FLIGHT
            && port->dev_type[id] != DEV_PSX_PS_2_KB_MOUSE_ADAPTER
            && (port->dev_id[id] == 0x41 || port->dev_id[id] == 0x73)) {
        if (wired_adapter.data[id + port->mt_first_port].output[18]) {
            if (!port->analog_btn[id]) {
                port->analog_btn[id] = 1;
            }
        }
        else {
            if (port->analog_btn[id]) {
                port->analog_btn[id] = 0;
                port->rumble_r_state[id] = 0;
                port->rumble_l_state[id] = 0;
                if (port->dev_id[id] == 0x41) {
                    port->dev_id[id] = 0x73;
                    port->dev_desc[id] = 0x3FFFF;
                    if (id == 0) {
                        gpio_set_level_iram(ps_ctrl_ports[port->mt_first_port ? 1 : 0].led_pin, 1);
                    }
                }
                else {
                    port->dev_id[id] = 0x41;
                    port->dev_desc[id] = 0;
                    if (id == 0) {
                        gpio_set_level_iram(ps_ctrl_ports[port->mt_first_port ? 1 : 0].led_pin, 0);
                    }
                }
            }
        }
    }
}

static void ps_cmd_req_hdlr(struct ps_ctrl_port *port, uint8_t id, uint8_t cmd, uint8_t *req) {
    switch (cmd) {
        case 0x42:
        {
            if (port->dev_id[id] != 0x41 && config.out_cfg[id + port->mt_first_port].acc_mode == ACC_RUMBLE) {
                struct raw_fb fb_data = {0};
                req++;
                if (port->rumble_r_state[id]) {
                    fb_data.data[0] = (req[port->rumble_r_idx[id]]) ? 1 : 0;
                }
                if (!fb_data.data[0] && port->rumble_l_state[id]) {
                    fb_data.data[0] = (req[port->rumble_l_idx[id]]) ? 1 : 0;
                }
                if (port->last_rumble[id] != fb_data.data[0]) {
                    port->last_rumble[id] = fb_data.data[0];
                    fb_data.header.wired_id = id + port->mt_first_port;
                    fb_data.header.type = FB_TYPE_RUMBLE;
                    fb_data.header.data_len = 1;
                    adapter_q_fb(&fb_data);
                }
            }
            break;
        }
        case 0x43:
        {
            if (req[1] == 0x01) {
                if (port->dev_id[id] != 0xF3) {
                    port->pend_dev_id[id] = port->dev_id[id];
                    port->dev_id[id] = 0xF3;
                }
            }
            else {
                port->dev_id[id] = port->pend_dev_id[id];
            }
            break;
        }
        case 0x44:
        {
            if (port->dev_id[id] == 0xF3) {
                port->rumble_r_state[id] = 0;
                port->rumble_l_state[id] = 0;
                if (req[1] == 0x01) {
                    port->pend_dev_id[id] = 0x73;
                    port->dev_desc[id] = 0x3FFFF;
                    if (id == 0) {
                        gpio_set_level_iram(ps_ctrl_ports[port->mt_first_port ? 1 : 0].led_pin, 1);
                    }
                }
                else {
                    port->pend_dev_id[id] = 0x41;
                    port->dev_desc[id] = 0;
                    if (id == 0) {
                        gpio_set_level_iram(ps_ctrl_ports[port->mt_first_port ? 1 : 0].led_pin, 0);
                    }
                }
            }
            break;
        }
        case 0x4D:
        {
            if (port->dev_id[id] == 0xF3) {
                req++;
                port->rumble_r_state[id] = 0;
                port->rumble_l_state[id] = 0;
                for (uint32_t i = 0; i < 6; i++) {
                    if (req[i] == 0x00) {
                        port->rumble_r_state[id] = 1;
                        port->rumble_r_idx[id] = i;
                    }
                    else if (req[i] == 0x01) {
                        port->rumble_l_state[id] = 1;
                        port->rumble_l_idx[id] = i;
                    }
                }
            }
            break;
        }
        case 0x4F:
        {
            if (port->dev_id[id] == 0xF3) {
                req++;
                port->dev_desc[id] = *(uint32_t *)req;
                if (port->dev_desc[id] & 0x3C0) {
                    port->pend_dev_id[id] = 0x79;
                }
                else {
                    port->pend_dev_id[id] = 0x73;
                }
            }
            break;
        }
    }
}

static void ps_cmd_rsp_hdlr(struct ps_ctrl_port *port, uint8_t id, uint8_t cmd, uint8_t *rsp) {
    *rsp++ = 0x5A;
    switch (cmd) {
        case 0x40:
        {
            *rsp++ = 0x00;
            *rsp++ = 0x00;
            *rsp++ = 0x02;
            *rsp++ = 0x00;
            *rsp++ = 0x00;
            *rsp++ = 0x5A;
            break;
        }
        case 0x41:
        {
            *(uint32_t *)rsp = port->dev_desc[id];
            rsp += 4;
            *rsp++ = 0x00;
            *rsp++ = (port->pend_dev_id[id] == 0x41) ? 0x00 : 0x5A;
            break;
        }
        case 0x44:
        {
            memset(rsp, 0x00, 6);
            break;
        }
        case 0x45:
        {
            /* Always reporting DualShock2 should be fine? */
            *rsp++ = 0x03;
            *rsp++ = 0x02;
            *rsp++ = (port->pend_dev_id[id] == 0x41) ? 0x00 : 0x01;
            *rsp++ = 0x02;
            *rsp++ = 0x01;
            *rsp++ = 0x00;
            break;
        }
        case 0x46:
        {
            /* Handle first offset only here */
            *rsp++ = 0x00;
            *rsp++ = 0x00;
            *rsp++ = 0x01;
            *rsp++ = 0x02;
            *rsp++ = 0x00;
            *rsp++ = 0x0A;
            break;
        }
        case 0x47:
        {
            *rsp++ = 0x00;
            *rsp++ = 0x00;
            *rsp++ = 0x02;
            *rsp++ = 0x00;
            *rsp++ = 0x01;
            *rsp++ = 0x00;
            break;
        }
        case 0x4C:
        {
            /* Handle first offset only here */
            *rsp++ = 0x00;
            *rsp++ = 0x00;
            *rsp++ = 0x00;
            *rsp++ = 0x04;
            *rsp++ = 0x00;
            *rsp++ = 0x00;
            break;
        }
        case 0x4D:
        {
            memset(rsp, 0xFF, 6);
            if (port->rumble_r_state[id]) {
                rsp[port->rumble_r_idx[id]] = 0x00;
            }
            if (port->rumble_l_state[id]) {
                rsp[port->rumble_l_idx[id]] = 0x01;
            }
            break;
        }
        case 0x4F:
        {
            memset(rsp, 0x00, 5);
            rsp[5] = 0x5A;
            break;
        }
        case 0x43:
        {
            if (port->dev_id[id] == 0xF3) {
                memset(rsp, 0x00, 6);
                break;
            }
            /* Fallthrough to 0x42 if not in cfg mode */
        }
        __attribute__ ((fallthrough));
        case 0x42:
        default:
        {
            uint32_t size = (port->dev_id[id] & 0xF) * 2;
            if (!(port->root_dev_type == DEV_PSX_MULTITAP && port->mt_state)) {
                port->tx_buf_len = 3 + size;
            }
            if (size < 6) {
                size = 6;
            }
            switch (port->dev_type[id]) {
                case DEV_PSX_PS_2_KB_MOUSE_ADAPTER:
                {
                    uint32_t len = 0;
                    memset(rsp, 0x00, size);
                    (void)kbmon_get_code(id + port->mt_first_port, rsp, &len);
                    break;
                }
                case DEV_PSX_MOUSE:
                    memcpy(rsp, wired_adapter.data[id + port->mt_first_port].output, 2);
                    load_mouse_axes(id + port->mt_first_port, &rsp[2]);
                    break;
                default:
                    *(uint16_t *)&rsp[0] = wired_adapter.data[id + port->mt_first_port].output16[0]
                        | wired_adapter.data[id + port->mt_first_port].output_mask16[0];
                    if (size >  2) {
                        for (uint32_t i = 2; i < 6; ++i) {
                            rsp[i] = (wired_adapter.data[id + port->mt_first_port].output_mask[i]) ?
                                wired_adapter.data[id + port->mt_first_port].output_mask[i]
                                : wired_adapter.data[id + port->mt_first_port].output[i];
                        }
                    }
                    if (size > 6) {
                        *(uint16_t *)&rsp[6] = wired_adapter.data[id + port->mt_first_port].output16[3]
                            & wired_adapter.data[id + port->mt_first_port].output_mask16[3];
                        *(uint32_t *)&rsp[8] = wired_adapter.data[id + port->mt_first_port].output32[2]
                            & wired_adapter.data[id + port->mt_first_port].output_mask32[2];
                        *(uint32_t *)&rsp[12] = wired_adapter.data[id + port->mt_first_port].output32[3]
                            & wired_adapter.data[id + port->mt_first_port].output_mask32[3];
                        *(uint16_t *)&rsp[16] = wired_adapter.data[id + port->mt_first_port].output16[8]
                            & wired_adapter.data[id + port->mt_first_port].output_mask16[8];
                    }
                    ++wired_adapter.data[id + port->mt_first_port].frame_cnt;
                    break;
            }
            if (cmd != 0x42 && cmd != 0x43) {
                ets_printf("# P%d Ukn: %02X\n", id, cmd);
            }
            break;
        }
    }
}

static void ps_cmd_const_rsp_hdlr(struct ps_ctrl_port *port, uint8_t id, uint8_t cmd, uint8_t *rsp) {
    switch (cmd) {
        case 0x46:
        {
            /* Handle 2nd offset only here */
            *rsp++ = 0x00;
            *rsp++ = 0x01;
            *rsp++ = 0x01;
            *rsp++ = 0x01;
            *rsp++ = 0x14;
            break;
        }
        case 0x4C:
        default:
        {
            /* Handle 2nd offset only here */
            *rsp++ = 0x00;
            *rsp++ = 0x00;
            *rsp++ = 0x07;
            *rsp++ = 0x00;
            *rsp++ = 0x00;
            break;
        }
    }
}

static void packet_end(void *arg) {
    const uint32_t low_io = GPIO.acpu_int;
    const uint32_t high_io = GPIO.acpu_int1.intr;
    uint32_t port_int[2] = {0};

    if (high_io & BIT(P1_DTR_PIN - 32)) {
        port_int[0] = 1;
    }
    if (low_io & BIT(P2_DTR_PIN)) {
        port_int[1] = 1;
    }

    for (uint32_t i = 0; i < PS_PORT_MAX; i++) {
        if (port_int[i]) {
            struct ps_ctrl_port *port = &ps_ctrl_ports[i];
            set_output_state(port->id, 0);
            port->idx = 0;
            port->mt_idx = 0;
            port->spi_hw->slave.trans_inten = 1;
            port->spi_hw->slave.trans_done = 0;
            if (port->valid) {
                if (port->root_dev_type == DEV_PSX_MULTITAP && port->mt_state) {
                    uint8_t prev_rx_buf = port->active_rx_buf ^ 0x01;
                    for (uint32_t j = 0; j < MT_PORT_MAX; j++) {
                        ps_analog_btn_hdlr(port, j);
                        ps_cmd_req_hdlr(port, j, port->rx_buf[prev_rx_buf][3 + (j * 8)], &port->rx_buf[prev_rx_buf][4 + (j * 8)]);
                        ps_gen_turbo_mask(&wired_adapter.data[port->mt_first_port + j]);
                    }
                }
                else {
                    ps_analog_btn_hdlr(port, 0);
                    ps_cmd_req_hdlr(port, 0, port->rx_buf[port->active_rx_buf][1], &port->rx_buf[port->active_rx_buf][2]);
                    ps_gen_turbo_mask(&wired_adapter.data[port->id]);
                }
                if (port->root_dev_type == DEV_PSX_MULTITAP) {
                    port->mt_state = port->rx_buf[port->active_rx_buf][2];
                }
                port->valid = 0;
                port->active_rx_buf ^= 0x01;
            }
            else if (port->game_id_len) {
                struct raw_fb fb_data = {0};
                uint8_t offset = (port->rx_buf[port->active_rx_buf][4] == 'c') ? 7 : 0;
                uint8_t len = port->game_id_len - offset;

                offset += 4;

                uint8_t *str = &port->rx_buf[port->active_rx_buf][offset];

                if (len > 13) {
                    len = 13;
                }

                fb_data.header.wired_id = 0;
                fb_data.header.type = FB_TYPE_GAME_ID;
                fb_data.header.data_len = len;
                for (uint32_t i = 0; i < len; ++i) {
                    fb_data.data[i] = str[i];
                }
                adapter_q_fb(&fb_data);

                port->game_id_len = 0;
            }
        }
    }

    if (high_io) GPIO.status1_w1tc.intr_st = high_io;
    if (low_io) GPIO.status_w1tc = low_io;
}

static void spi_isr(void* arg) {
    struct ps_ctrl_port *port = (struct ps_ctrl_port *)arg;
    if (port->root_dev_type == DEV_PSX_MULTITAP && port->mt_state && port->idx > 10) {
        delay_us(0);
    }
    else {
        delay_us(14);
    }
    port->rx_buf[port->active_rx_buf][port->idx] = port->spi_hw->data_buf[0];
    if (!get_dtr_state(port->id)) {
        if (port->idx == 0) {
            port->tx_buf_len = 3 + 6;
            if (port->rx_buf[port->active_rx_buf][0] == 0x01) {
                set_output_state(port->id, 1);
                port->valid = 1;
                if (port->root_dev_type == DEV_PSX_MULTITAP && port->mt_state) {
                    port->tx_buf_len = 3 + (2 + 6) * 4;
                    port->tx_buf[1] = 0x80;
                    port->tx_buf[2] = 0x5A;
                }
                else {
                    port->tx_buf[1] = port->dev_id[0];
                }
            }
            else {
                port->valid = 0;
                if (port->rx_buf[port->active_rx_buf][0] != 0x81) {
                    port->spi_hw->slave.trans_inten = 0;
                    goto early_end;
                }
            }
        }
        /* GAME ID sent to memcard */
        else if (!port->valid) {
            if (port->idx == 1) {
                if (port->rx_buf[port->active_rx_buf][1] != 0x21) {
                    port->spi_hw->slave.trans_inten = 0;
                    goto early_end;
                }
                port->game_id_len = 0;
            }
            else if (port->idx == 3) {
                port->game_id_len = port->rx_buf[port->active_rx_buf][3];
            }
        }
        else if (port->root_dev_type == DEV_PSX_MULTITAP && port->mt_state) {
            uint8_t prev_rx_buf = port->active_rx_buf ^ 0x01;
            if (port->idx == 3 && port->rx_buf[port->active_rx_buf][2] == 0x00) {
                port->mt_state = 0;
                port->valid = 0;
                port->spi_hw->slave.trans_inten = 0;
                goto early_end;
            }
            else if ((port->idx - 2) % 8  == 0) {
                port->tx_buf[port->idx + 1] = port->dev_id[port->mt_idx];
            }
            else if ((port->idx - 3) % 8  == 0) {
                ps_cmd_rsp_hdlr(port, port->mt_idx++, port->rx_buf[prev_rx_buf][port->idx], &port->tx_buf[port->idx + 1]);
            }
            else if ((port->idx - 5) % 8  == 0
                    && (port->rx_buf[prev_rx_buf][port->idx - 2] == 0x46 || port->rx_buf[prev_rx_buf][port->idx - 2] == 0x4C)
                    && port->rx_buf[prev_rx_buf][port->idx] == 0x01) {
                ps_cmd_const_rsp_hdlr(port, port->mt_idx - 1, port->rx_buf[prev_rx_buf][port->idx - 2], &port->tx_buf[port->idx + 1]);
            }
        }
        else {
            if (port->idx == 1) {
                /* Special dev only ACK 0x42 */
                if ((port->root_dev_type == DEV_PSX_MOUSE || port->root_dev_type == DEV_PSX_FLIGHT || port->root_dev_type == DEV_PSX_PS_2_KB_MOUSE_ADAPTER)
                    && port->rx_buf[port->active_rx_buf][1] != 0x42) {
                    port->valid = 0;
                    port->spi_hw->slave.trans_inten = 0;
                    goto early_end;
                }
                else {
                    ps_cmd_rsp_hdlr(port, 0, port->rx_buf[port->active_rx_buf][1], &port->tx_buf[2]);
                }
            }
            else if (port->idx == 3 && (port->rx_buf[port->active_rx_buf][1] == 0x46 || port->rx_buf[port->active_rx_buf][1] == 0x4C)
                    && port->rx_buf[port->active_rx_buf][3] == 0x01) {
                ps_cmd_const_rsp_hdlr(port, 0, port->rx_buf[port->active_rx_buf][1], &port->tx_buf[4]);
            }
        }
        port->spi_hw->data_buf[0] = port->tx_buf[++port->idx];
        port->spi_hw->slave.sync_reset = 1;
        port->spi_hw->slave.trans_done = 0;
        port->spi_hw->cmd.usr = 1;
        if (port->valid && port->idx < port->tx_buf_len) {
            toggle_dsr(port->id);
        }
        return;
    }
early_end:
    port->spi_hw->slave.sync_reset = 1;
    port->spi_hw->slave.trans_done = 0;
    port->spi_hw->cmd.usr = 1;
}

static unsigned isr_dispatch(unsigned cause) {
    if (cause & SPI2_HSPI_INTR_MASK) {
        spi_isr((void *)&ps_ctrl_ports[0]);
    }
    if (cause & SPI3_VSPI_INTR_MASK) {
        spi_isr((void *)&ps_ctrl_ports[1]);
    }
    if (cause & GPIO_INTR_MASK) {
        packet_end(NULL);
    }
    return 0;
}

void ps_spi_init(uint32_t package) {
    for (uint32_t i = 0; i < PS_PORT_MAX; i++) {
        memset(ps_ctrl_ports[i].dev_id, 0x41, sizeof(ps_ctrl_ports[0].dev_id));
        memset(ps_ctrl_ports[i].pend_dev_id, 0x41, sizeof(ps_ctrl_ports[0].pend_dev_id));
        memset(ps_ctrl_ports[i].tx_buf, 0xFF, sizeof(ps_ctrl_ports[0].tx_buf));
        ps_ctrl_ports[i].rx_buf[0][0] = 0x01;
        ps_ctrl_ports[i].rx_buf[1][0] = 0x01;
        ps_ctrl_ports[i].rx_buf[0][1] = 0x42;
        ps_ctrl_ports[i].rx_buf[1][1] = 0x42;
        ps_ctrl_ports[i].rx_buf[0][2] = 0x01;
        ps_ctrl_ports[i].rx_buf[1][2] = 0x01;
        ps_ctrl_ports[i].rx_buf[0][3] = 0x42;
        ps_ctrl_ports[i].rx_buf[1][3] = 0x42;
        ps_ctrl_ports[i].rx_buf[0][11] = 0x42;
        ps_ctrl_ports[i].rx_buf[1][11] = 0x42;
        ps_ctrl_ports[i].rx_buf[0][19] = 0x42;
        ps_ctrl_ports[i].rx_buf[1][19] = 0x42;
        ps_ctrl_ports[i].rx_buf[0][27] = 0x42;
        ps_ctrl_ports[i].rx_buf[1][27] = 0x42;
        for (uint32_t j = 0; j < MT_PORT_MAX; j++) {
             ps_ctrl_ports[i].dev_desc[j] = 0;
        }
    }

    /* Setup device */
    switch (config.global_cfg.multitap_cfg) {
        case MT_SLOT_1:
            ps_ctrl_ports[0].mt_state = 0x01;
            ps_ctrl_ports[0].root_dev_type = DEV_PSX_MULTITAP;
            ps_ctrl_ports[1].mt_first_port = MT_PORT_MAX;
            break;
        case MT_SLOT_2:
            ps_ctrl_ports[1].mt_state = 0x01;
            ps_ctrl_ports[1].root_dev_type = DEV_PSX_MULTITAP;
            ps_ctrl_ports[1].mt_first_port = 1;
            break;
        case MT_DUAL:
            ps_ctrl_ports[0].mt_state = 0x01;
            ps_ctrl_ports[0].root_dev_type = DEV_PSX_MULTITAP;
            ps_ctrl_ports[1].mt_state = 0x01;
            ps_ctrl_ports[1].root_dev_type = DEV_PSX_MULTITAP;
            ps_ctrl_ports[1].mt_first_port = MT_PORT_MAX;
            break;
        default:
            ps_ctrl_ports[1].mt_first_port = 1;
    }
    for (uint32_t i = 0; i < PS_PORT_MAX; i++) {
        for (uint32_t j = 0; j < MT_PORT_MAX; j++) {
            switch (config.out_cfg[ps_ctrl_ports[i].mt_first_port + j].dev_mode) {
                case DEV_PAD:
                    ps_ctrl_ports[i].dev_id[j] = 0x41;
                    ps_ctrl_ports[i].dev_type[j] = DEV_PS2_DS2;
                    if (!ps_ctrl_ports[i].root_dev_type) {
                        ps_ctrl_ports[i].root_dev_type = DEV_PS2_DS2;
                        goto inner_break;
                    }
                    break;
                case DEV_PAD_ALT:
                    ps_ctrl_ports[i].dev_id[j] = 0x53;
                    ps_ctrl_ports[i].dev_type[j] = DEV_PSX_FLIGHT;
                    if (!ps_ctrl_ports[i].root_dev_type) {
                        ps_ctrl_ports[i].root_dev_type = DEV_PSX_FLIGHT;
                        goto inner_break;
                    }
                    break;
                case DEV_KB:
                    ps_ctrl_ports[i].dev_id[j] = 0x96;
                    ps_ctrl_ports[i].dev_type[j] = DEV_PSX_PS_2_KB_MOUSE_ADAPTER;
                    if (!ps_ctrl_ports[i].root_dev_type) {
                        ps_ctrl_ports[i].root_dev_type = DEV_PSX_PS_2_KB_MOUSE_ADAPTER;
                        goto inner_break;
                    }
                    break;
                case DEV_MOUSE:
                    ps_ctrl_ports[i].dev_id[j] = 0x12;
                    ps_ctrl_ports[i].dev_type[j] = DEV_PSX_MOUSE;
                    if (!ps_ctrl_ports[i].root_dev_type) {
                        ps_ctrl_ports[i].root_dev_type = DEV_PSX_MOUSE;
                        goto inner_break;
                    }
                    break;
            }
        }
inner_break:
        ;
    }

    /* DTR */
    ps_spi_port_cfg(0x3);

    /* DSR */
    gpio_set_level_iram(P1_DSR_PIN, 1);
    gpio_set_direction_iram(P1_DSR_PIN, GPIO_MODE_INPUT);
    gpio_set_level_iram(P2_DSR_PIN, 1);
    gpio_set_direction_iram(P2_DSR_PIN, GPIO_MODE_INPUT);

    /* RXD */
    gpio_set_level_iram(P1_RXD_PIN, 1);
    gpio_set_direction_iram(P1_RXD_PIN, GPIO_MODE_INPUT);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[P1_RXD_PIN], PIN_FUNC_GPIO);
    gpio_set_level_iram(P2_RXD_PIN, 1);
    gpio_set_direction_iram(P2_RXD_PIN, GPIO_MODE_INPUT);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[P2_RXD_PIN], PIN_FUNC_GPIO);

    /* TXD */
    gpio_set_pull_mode_iram(P1_TXD_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction_iram(P1_TXD_PIN, GPIO_MODE_INPUT);
    gpio_matrix_in(P1_TXD_PIN, HSPID_IN_IDX, false);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[P1_TXD_PIN], PIN_FUNC_GPIO);
    gpio_set_pull_mode_iram(P2_TXD_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction_iram(P2_TXD_PIN, GPIO_MODE_INPUT);
    gpio_matrix_in(P2_TXD_PIN, VSPID_IN_IDX, false);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[P2_TXD_PIN], PIN_FUNC_GPIO);

    /* SCK */
    gpio_set_pull_mode_iram(P1_SCK_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction_iram(P1_SCK_PIN, GPIO_MODE_INPUT);
    gpio_matrix_in(P1_SCK_PIN, HSPICLK_IN_IDX, false);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[P1_SCK_PIN], PIN_FUNC_GPIO);
    gpio_set_pull_mode_iram(P2_SCK_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction_iram(P2_SCK_PIN, GPIO_MODE_INPUT);
    gpio_matrix_in(P2_SCK_PIN, VSPICLK_IN_IDX, false);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG_IRAM[P2_SCK_PIN], PIN_FUNC_GPIO);

    periph_ll_enable_clk_clear_rst(PERIPH_HSPI_MODULE);
    periph_ll_enable_clk_clear_rst(PERIPH_VSPI_MODULE);

    for (uint32_t i = 0; i < PS_PORT_MAX; i++) {
        spi_init(&ps_ctrl_ports[i].cfg);
    }

    intexc_alloc_iram(ETS_SPI2_INTR_SOURCE, SPI2_HSPI_INTR_NUM, isr_dispatch);
    intexc_alloc_iram(ETS_SPI3_INTR_SOURCE, SPI3_VSPI_INTR_NUM, isr_dispatch);
    intexc_alloc_iram(ETS_GPIO_INTR_SOURCE, GPIO_INTR_NUM, isr_dispatch);
}

void ps_spi_port_cfg(uint16_t mask) {
    uint32_t signals[PS_PORT_MAX] = {HSPICS0_IN_IDX, VSPICS0_IN_IDX};
    uint32_t gpio_pin[PS_PORT_MAX] = {P1_DTR_PIN, P2_DTR_PIN};

    for (uint32_t i = 0; i < PS_PORT_MAX; i++) {

        if (mask & 0x1) {
            gpio_config_t io_conf = {
                .mode = GPIO_MODE_INPUT,
                .intr_type = GPIO_PIN_INTR_POSEDGE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .pull_up_en = GPIO_PULLUP_DISABLE,
            };

            io_conf.pin_bit_mask = 1ULL << gpio_pin[i];
            gpio_config_iram(&io_conf);
            gpio_matrix_in(gpio_pin[i], signals[i], 0);
        }
        else {
            gpio_reset_iram(gpio_pin[i]);
            gpio_matrix_in(GPIO_MATRIX_CONST_ONE_INPUT, signals[i], 0);
        }
        mask >>= 1;
    }
}
