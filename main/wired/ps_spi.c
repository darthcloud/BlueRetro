/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <driver/periph_ctrl.h>
#include <driver/gpio.h>
#include <soc/spi_periph.h>
#include <esp32/rom/ets_sys.h>
#include "zephyr/atomic.h"
#include "zephyr/types.h"
#include "util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "adapter/kb_monitor.h"
#include "adapter/ps.h"
#include "ps_spi.h"

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

#define PS_SPI_DTR 0
#define PS_SPI_SCK 1
#define PS_SPI_TXD 2
#define PS_SPI_RXD 3
#define PS_SPI_DSR 4

#define PS_BUFFER_SIZE 64
#define PS_PORT_MAX 2
#define MT_PORT_MAX 4

#define SPI_LL_RST_MASK (SPI_OUT_RST | SPI_IN_RST | SPI_AHBM_RST | SPI_AHBM_FIFO_RST)
#define SPI_LL_UNUSED_INT_MASK  (SPI_INT_EN | SPI_SLV_WR_STA_DONE | SPI_SLV_RD_STA_DONE | SPI_SLV_WR_BUF_DONE | SPI_SLV_RD_BUF_DONE)

struct ps_ctrl_port {
    spi_dev_t *spi_hw;
    uint8_t id;
    uint8_t dev_type;
    uint8_t mt_state;
    uint8_t mt_first_port;
    uint8_t tx_buf[PS_BUFFER_SIZE];
    uint8_t rx_buf[2][PS_BUFFER_SIZE];
    uint8_t active_rx_buf;
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
};

static struct ps_ctrl_port ps_ctrl_ports[PS_PORT_MAX] = {0};

static uint32_t IRAM_ATTR get_dtr_state(uint32_t port) {
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

static void IRAM_ATTR set_output_state(uint32_t port, uint32_t enable) {
    if (port == 0) {
        if (enable) {
            gpio_set_direction(P1_DSR_PIN, GPIO_MODE_OUTPUT);
            gpio_set_direction(P1_RXD_PIN, GPIO_MODE_OUTPUT);
            gpio_matrix_out(P1_RXD_PIN, spi_periph_signal[ESP_SPI2_HSPI].spiq_out, false, false);
        }
        else {
            gpio_set_direction(P1_RXD_PIN, GPIO_MODE_INPUT);
            gpio_set_direction(P1_DSR_PIN, GPIO_MODE_INPUT);
        }
    }
    else {
        if (enable) {
            gpio_set_direction(P2_DSR_PIN, GPIO_MODE_OUTPUT);
            gpio_set_direction(P2_RXD_PIN, GPIO_MODE_OUTPUT);
            gpio_matrix_out(P2_RXD_PIN, spi_periph_signal[ESP_SPI3_VSPI].spiq_out, false, false);
        }
        else {
            gpio_set_direction(P2_RXD_PIN, GPIO_MODE_INPUT);
            gpio_set_direction(P2_DSR_PIN, GPIO_MODE_INPUT);
        }
    }
}

static void IRAM_ATTR toggle_dsr(uint32_t port) {
    if (port == 0) {
        GPIO.out_w1tc = BIT(P1_DSR_PIN);
        ets_delay_us(2);
        GPIO.out_w1ts = BIT(P1_DSR_PIN);
    }
    else {
        GPIO.out_w1tc = BIT(P2_DSR_PIN);
        ets_delay_us(2);
        GPIO.out_w1ts = BIT(P2_DSR_PIN);
    }
}

static void IRAM_ATTR ps_analog_btn_hdlr(struct ps_ctrl_port *port, uint8_t id) {
    if (port->dev_type != DEV_PSX_MOUSE && port->dev_type != DEV_PSX_FLIGHT && port->dev_type != DEV_PSX_PS_2_KB_MOUSE_ADAPTER) {
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
                }
                else {
                    port->dev_id[id] = 0x41;
                }
            }
        }
    }
}

static void IRAM_ATTR ps_cmd_req_hdlr(struct ps_ctrl_port *port, uint8_t id, uint8_t cmd, uint8_t *req) {
    switch (cmd) {
        case 0x42:
        {
            if (port->dev_id[id] != 0x41 && config.out_cfg[id].acc_mode == ACC_RUMBLE) {
                uint8_t rumble_data[2] = {0};
                req++;
                if (port->rumble_r_state) {
                    rumble_data[1] = (req[port->rumble_r_idx[id]]) ? 1 : 0;
                }
                if (!rumble_data[1] && port->rumble_l_state) {
                    rumble_data[1] = (req[port->rumble_l_idx[id]]) ? 1 : 0;
                }
                if (port->last_rumble[id] != rumble_data[1]) {
                    port->last_rumble[id] = rumble_data[1];
                    rumble_data[0] = id + port->mt_first_port;
                    adapter_q_fb(rumble_data, 2);
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
                }
                else {
                    port->pend_dev_id[id] = 0x41;
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

static void IRAM_ATTR ps_cmd_rsp_hdlr(struct ps_ctrl_port *port, uint8_t id, uint8_t cmd, uint8_t *rsp) {
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
            *rsp++ = 0x5A;
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
        {
            uint32_t size = (port->dev_id[id] & 0xF) * 2;
            if (size < 6) {
                size = 6;
            }
            if (port->dev_type == DEV_PSX_PS_2_KB_MOUSE_ADAPTER) {
                uint32_t len = 0;
                memset(rsp, 0x00, size);
                kbmon_get_code(id + port->mt_first_port, rsp, &len);
            }
            else {
                memcpy(rsp, wired_adapter.data[id + port->mt_first_port].output, size);
            }
            if (port->dev_type == DEV_PSX_MOUSE) {
                wired_adapter.data[id + port->mt_first_port].output[2] = 0x00;
                wired_adapter.data[id + port->mt_first_port].output[3] = 0x00;
            }
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
        default:
            ets_printf("%02X: Unk cmd: 0x%02X\n", id, cmd);
    }
}

static void IRAM_ATTR ps_cmd_const_rsp_hdlr(struct ps_ctrl_port *port, uint8_t id, uint8_t cmd, uint8_t *rsp) {
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

static void IRAM_ATTR packet_end(void* arg) {
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
                if (port->dev_type == DEV_PSX_MULTITAP && port->mt_state) {
                    uint8_t prev_rx_buf = port->active_rx_buf ^ 0x01;
                    for (uint32_t j = 0; j < MT_PORT_MAX; j++) {
                        ps_analog_btn_hdlr(port, j);
                        ps_cmd_req_hdlr(port, j, port->rx_buf[prev_rx_buf][3 + (j * 8)], &port->rx_buf[prev_rx_buf][4 + (j * 8)]);
                    }
                }
                else {
                    ps_analog_btn_hdlr(port, 0);
                    ps_cmd_req_hdlr(port, 0, port->rx_buf[port->active_rx_buf][1], &port->rx_buf[port->active_rx_buf][2]);
                }
                if (port->dev_type == DEV_PSX_MULTITAP) {
                    port->mt_state = port->rx_buf[port->active_rx_buf][2];
                }
                port->valid = 0;
                port->active_rx_buf ^= 0x01;
            }
        }
    }

    if (high_io) GPIO.status1_w1tc.intr_st = high_io;
    if (low_io) GPIO.status_w1tc = low_io;
}

static void IRAM_ATTR spi_isr(void* arg) {
    struct ps_ctrl_port *port = (struct ps_ctrl_port *)arg;
    if (port->dev_type == DEV_PSX_MULTITAP && port->mt_state && port->idx > 10) {
        ets_delay_us(0);
    }
    else {
        ets_delay_us(14);
    }
    port->rx_buf[port->active_rx_buf][port->idx] = port->spi_hw->data_buf[0];
    if (!get_dtr_state(port->id)) {
        if (port->idx == 0) {
            if (port->rx_buf[port->active_rx_buf][0] == 0x01) {
                set_output_state(port->id, 1);
                port->valid = 1;
                if (port->dev_type == DEV_PSX_MULTITAP && port->mt_state) {
                    port->tx_buf[1] = 0x80;
                    port->tx_buf[2] = 0x5A;
                }
                else {
                    port->tx_buf[1] = port->dev_id[0];
                }
            }
            else {
                port->valid = 0;
                port->spi_hw->slave.trans_inten = 0;
                goto early_end;
            }
        }
        else if (port->dev_type == DEV_PSX_MULTITAP && port->mt_state) {
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
                if ((port->dev_type == DEV_PSX_MOUSE || port->dev_type == DEV_PSX_FLIGHT || port->dev_type == DEV_PSX_PS_2_KB_MOUSE_ADAPTER)
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
        toggle_dsr(port->id);
        return;
    }
early_end:
    port->spi_hw->slave.sync_reset = 1;
    port->spi_hw->slave.trans_done = 0;
    port->spi_hw->cmd.usr = 1;
}

void ps_spi_init(void) {
    gpio_config_t io_conf = {0};

    ps_ctrl_ports[0].id = 0;
    ps_ctrl_ports[1].id = 1;
    ps_ctrl_ports[0].spi_hw = &SPI2;
    ps_ctrl_ports[1].spi_hw = &SPI3;


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
             ps_ctrl_ports[i].dev_desc[j] = 0x3FFFF;
        }
    }

    /* Setup device */
    switch (config.global_cfg.multitap_cfg) {
        case MT_SLOT_1:
            ps_ctrl_ports[0].mt_state = 0x01;
            ps_ctrl_ports[0].dev_type = DEV_PSX_MULTITAP;
            ps_ctrl_ports[1].mt_first_port = MT_PORT_MAX;
            break;
        case MT_SLOT_2:
            ps_ctrl_ports[1].mt_state = 0x01;
            ps_ctrl_ports[1].dev_type = DEV_PSX_MULTITAP;
            ps_ctrl_ports[1].mt_first_port = 1;
            break;
        case MT_DUAL:
            ps_ctrl_ports[0].mt_state = 0x01;
            ps_ctrl_ports[0].dev_type = DEV_PSX_MULTITAP;
            ps_ctrl_ports[1].mt_state = 0x01;
            ps_ctrl_ports[1].dev_type = DEV_PSX_MULTITAP;
            ps_ctrl_ports[1].mt_first_port = MT_PORT_MAX;
            break;
        default:
            ps_ctrl_ports[1].mt_first_port = 1;
    }
    for (uint32_t i = 0; i < PS_PORT_MAX; i++) {
        if (ps_ctrl_ports[i].dev_type == DEV_NONE) {
            switch (config.out_cfg[ps_ctrl_ports[i].mt_first_port].dev_mode) {
                case DEV_PAD:
                    ps_ctrl_ports[i].dev_type = DEV_PS2_DS2;
                    break;
                case DEV_PAD_ALT:
                    ps_ctrl_ports[i].dev_type = DEV_PSX_FLIGHT;
                    ps_ctrl_ports[i].dev_id[0] = 0x53;
                    break;
                case DEV_KB:
                    ps_ctrl_ports[i].dev_type = DEV_PSX_PS_2_KB_MOUSE_ADAPTER;
                    ps_ctrl_ports[i].dev_id[0] = 0x96;
                    kbmon_init(i, ps_kb_id_to_scancode);
                    kbmon_set_typematic(i, 1, 500000, 90000);
                    break;
                case DEV_MOUSE:
                    ps_ctrl_ports[i].dev_type = DEV_PSX_MOUSE;
                    ps_ctrl_ports[i].dev_id[0] = 0x12;
                    break;
            }
        }
    }

    /* DTR */
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = 1ULL << P1_DTR_PIN;
    gpio_config(&io_conf);
    io_conf.pin_bit_mask = 1ULL << P2_DTR_PIN;
    gpio_config(&io_conf);
    gpio_matrix_in(P1_DTR_PIN, spi_periph_signal[ESP_SPI2_HSPI].spics_in, false);
    gpio_matrix_in(P2_DTR_PIN, spi_periph_signal[ESP_SPI3_VSPI].spics_in, false);

    /* DSR */
    gpio_set_level(P1_DSR_PIN, 1);
    gpio_set_direction(P1_DSR_PIN, GPIO_MODE_INPUT);
    gpio_set_level(P2_DSR_PIN, 1);
    gpio_set_direction(P2_DSR_PIN, GPIO_MODE_INPUT);

    /* RXD */
    gpio_set_level(P1_RXD_PIN, 1);
    gpio_set_direction(P1_RXD_PIN, GPIO_MODE_INPUT);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[P1_RXD_PIN], PIN_FUNC_GPIO);
    gpio_set_level(P2_RXD_PIN, 1);
    gpio_set_direction(P2_RXD_PIN, GPIO_MODE_INPUT);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[P2_RXD_PIN], PIN_FUNC_GPIO);

    /* TXD */
    gpio_set_pull_mode(P1_TXD_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction(P1_TXD_PIN, GPIO_MODE_INPUT);
    gpio_matrix_in(P1_TXD_PIN, spi_periph_signal[ESP_SPI2_HSPI].spid_in, false);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[P1_TXD_PIN], PIN_FUNC_GPIO);
    gpio_set_pull_mode(P2_TXD_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction(P2_TXD_PIN, GPIO_MODE_INPUT);
    gpio_matrix_in(P2_TXD_PIN, spi_periph_signal[ESP_SPI3_VSPI].spid_in, false);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[P2_TXD_PIN], PIN_FUNC_GPIO);

    /* SCK */
    gpio_set_pull_mode(P1_SCK_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction(P1_SCK_PIN, GPIO_MODE_INPUT);
    gpio_matrix_in(P1_SCK_PIN, spi_periph_signal[ESP_SPI2_HSPI].spiclk_in, false);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[P1_SCK_PIN], PIN_FUNC_GPIO);
    gpio_set_pull_mode(P2_SCK_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction(P2_SCK_PIN, GPIO_MODE_INPUT);
    gpio_matrix_in(P2_SCK_PIN, spi_periph_signal[ESP_SPI3_VSPI].spiclk_in, false);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[P2_SCK_PIN], PIN_FUNC_GPIO);


    periph_module_enable(PERIPH_HSPI_MODULE);
    periph_module_enable(PERIPH_VSPI_MODULE);

    for (uint32_t i = 0; i < PS_PORT_MAX; i++) {
        spi_dev_t *spi_hw = ps_ctrl_ports[i].spi_hw;

        spi_hw->clock.val = 0;
        spi_hw->user.val = 0;
        spi_hw->ctrl.val = 0;
        spi_hw->slave.wr_rd_buf_en = 1; //no sure if needed
        spi_hw->user.doutdin = 1; //we only support full duplex
        spi_hw->user.sio = 0;
        spi_hw->slave.slave_mode = 1;
        spi_hw->dma_conf.val |= SPI_LL_RST_MASK;
        spi_hw->dma_out_link.start = 0;
        spi_hw->dma_in_link.start = 0;
        spi_hw->dma_conf.val &= ~SPI_LL_RST_MASK;
        spi_hw->slave.sync_reset = 1;
        spi_hw->slave.sync_reset = 0;

        //use all 64 bytes of the buffer
        spi_hw->user.usr_miso_highpart = 0;
        spi_hw->user.usr_mosi_highpart = 0;

        //Disable unneeded ints
        spi_hw->slave.val &= ~SPI_LL_UNUSED_INT_MASK;

        /* PS is LSB first */
        spi_hw->ctrl.wr_bit_order = 1;
        spi_hw->ctrl.rd_bit_order = 1;

        /* Set Mode 3 as per ESP32 TRM, except ck_i_edge that need to be 1 for original PSX! */
        spi_hw->pin.ck_idle_edge = 0;
        spi_hw->user.ck_i_edge = 1;
        spi_hw->ctrl2.miso_delay_mode = 1;
        spi_hw->ctrl2.miso_delay_num = 0;
        spi_hw->ctrl2.mosi_delay_mode = 0;
        spi_hw->ctrl2.mosi_delay_num = 0;

        spi_hw->slave.sync_reset = 1;
        spi_hw->slave.sync_reset = 0;

        spi_hw->slv_wrbuf_dlen.bit_len = 8 - 1;
        spi_hw->slv_rdbuf_dlen.bit_len = 8 - 1;

        spi_hw->user.usr_miso = 1;
        spi_hw->user.usr_mosi = 1;

        spi_hw->data_buf[0] = 0xFF;

        spi_hw->slave.trans_inten = 1;
        spi_hw->slave.trans_done = 0;
        spi_hw->cmd.usr = 1;
    }

    esp_intr_alloc(spi_periph_signal[ESP_SPI2_HSPI].irq, 0, spi_isr, (void *)&ps_ctrl_ports[0], NULL);
    esp_intr_alloc(spi_periph_signal[ESP_SPI3_VSPI].irq, 0, spi_isr, (void *)&ps_ctrl_ports[1], NULL);
    esp_intr_alloc(ETS_GPIO_INTR_SOURCE, 0, packet_end, NULL, NULL);
}
