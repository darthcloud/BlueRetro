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

#define P1_TH_PIN 35
#define P1_TR_PIN 27
#define P1_TL_PIN 26
#define P1_R_PIN 23
#define P1_L_PIN 18
#define P1_D_PIN 5
#define P1_U_PIN 3

#define P2_TH_PIN 36
#define P2_TR_PIN 16
#define P2_TL_PIN 33
#define P2_R_PIN 25
#define P2_L_PIN 22
#define P2_D_PIN 21
#define P2_U_PIN 19

#define EA_CTRL_PIN 1
#define TP_CTRL_PIN 32

#define SIO_TH 0
#define SIO_TR 1
#define SIO_TL 2
#define SIO_R 3
#define SIO_L 4
#define SIO_D 5
#define SIO_U 6

#define ID0_GENESIS_PAD 0x00 //TBD
#define ID0_MOUSE 0x0B
#define ID0_GENESIS_MULTITAP 0x00 //TBD
#define ID0_SATURN_PAD 0x40
#define ID0_SATURN_THREEWIRE_HANDSHAKE 0x11
#define ID0_SATURN_CLOCKED_SERIAL 0x22
#define ID0_SATURN_CLOCKED_PARALLEL 0x33

#define ID1_MOUSE 0x3
#define ID1_SATURN_PERI 0x5
#define ID1_GENESIS_MULTITAP 0x7
#define ID1_SATURN_PAD 0xB
#define ID1_GENESIS_PAD 0xD
#define ID1_NON_CONNECTION 0xF

#define ID2_SATURN_PAD 0x0
#define ID2_SATURN_ANALOG_PAD 0x1
#define ID2_SATURN_POINTING 0x2
#define ID2_SATURN_KB 0x3
#define ID2_SATURN_MULTITAP 0x4
#define ID2_SATURN_MOUSE 0xE
#define ID2_NON_CONNECTION 0xF

#define TWH_TIMEOUT 4096
#define POLL_TIMEOUT 512

#define P1_OUT0_MASK (BIT(P1_TR_PIN) | BIT(P1_TL_PIN) | BIT(P1_R_PIN) | BIT(P1_L_PIN) | BIT(P1_D_PIN) | BIT(P1_U_PIN))
#define P1_OUT1_MASK 0
#define P2_OUT0_MASK (BIT(P2_TR_PIN) | BIT(P2_R_PIN) | BIT(P2_L_PIN) | BIT(P2_D_PIN) | BIT(P2_U_PIN))
#define P2_OUT1_MASK (BIT(P2_TL_PIN - 32))

#define SIX_BTNS_P1_C2_LO_MASK ~(BIT(P1_D_PIN) | BIT(P1_U_PIN))
#define SIX_BTNS_P2_C2_LO_MASK ~(BIT(P2_D_PIN) | BIT(P2_U_PIN))
#define SIX_BTNS_P1_C3_LO_MASK (BIT(P1_D_PIN) | BIT(P1_U_PIN) | BIT(P1_L_PIN) | BIT(P1_R_PIN))
#define SIX_BTNS_P2_C3_LO_MASK (BIT(P2_D_PIN) | BIT(P2_U_PIN) | BIT(P2_L_PIN) | BIT(P2_R_PIN))

#define MT_GEN_PORT_MAX 4
#define MT_PORT_MAX 6
enum {
    DEV_NONE = 0,
    DEV_GENESIS_3BTNS,
    DEV_GENESIS_6BTNS,
    DEV_GENESIS_MULTITAP,
    DEV_GENESIS_MOUSE,
    DEV_SATURN_DIGITAL,
    DEV_SATURN_DIGITAL_TWH,
    DEV_SATURN_ANALOG,
    DEV_SATURN_MULTITAP,
    DEV_SATURN_KB,
    DEV_EA_MULTITAP,
    DEV_TYPE_MAX,
};

static const uint8_t gen_ids[DEV_TYPE_MAX] = {
    0xF, 0x0, 0x1, 0xF, 0x2, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF,
};

static const uint8_t gpio_pin[2][7] = {
    {35, 27, 26, 23, 18,  5,  3},
    {36, 16, 33, 25, 22, 21, 19},
};

static const uint16_t gen_cycle_mask[8][6] = {
    {0x0010, 0x0020, 0x0000, 0x0000, 0x0004, 0x0008},
    {0x0010, 0x0020, 0x0040, 0x0080, 0x0001, 0x0002},
    {0x0010, 0x0020, 0x0000, 0x0000, 0x0004, 0x0008},
    {0x0010, 0x0020, 0x0040, 0x0080, 0x0001, 0x0002},
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0004, 0x0008},
    {0x1000, 0x2000, 0x4000, 0x8000, 0xFFFF, 0xFFFF},
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0004, 0x0008},
};

static uint8_t sel[2] = {0};
static uint8_t dev_type[2] = {0};
static uint8_t mt_dev_type[2][MT_PORT_MAX] = {0};
static uint8_t mt_first_port[2] = {0};
static uint8_t buffer[6*6];
static uint32_t *map1 = (uint32_t *)wired_adapter.data[0].output;
static uint32_t *map2 = (uint32_t *)wired_adapter.data[1].output;

#if 0
static uint8_t IRAM_ATTR get_id1(uint8_t val) {
    uint8_t id1 = 0;

    if ((val & 0x80) || (val & 0x40)) {
        id1 |= 0x8;
    }
    if ((val & 0x20) || (val & 0x10)) {
        id1 |= 0x4;
    }
    if ((val & 0x08) || (val & 0x04)) {
        id1 |= 0x2;
    }
    if ((val & 0x02) || (val & 0x01)) {
        id1 |= 0x1;
    }
    return id1;
}
#endif

static void IRAM_ATTR tx_nibble(uint8_t port, uint8_t data) {
    for (uint8_t i = SIO_R, mask = 0x8; mask; mask >>= 1, i++) {
        if (data & mask) {
            GPIO.out_w1ts = BIT(gpio_pin[port][i]);
        }
        else {
            GPIO.out_w1tc = BIT(gpio_pin[port][i]);
        }
    }
}

static void IRAM_ATTR set_sio(uint8_t port, uint8_t sio, uint8_t value) {
    uint8_t pin = gpio_pin[port][sio];

    if (pin < 32) {
        if (value) {
            GPIO.out_w1ts = BIT(pin);
        }
        else {
            GPIO.out_w1tc = BIT(pin);
        }
    }
    else {
        if (value) {
            GPIO.out1_w1ts.val = BIT(pin - 32);
        }
        else {
            GPIO.out1_w1tc.val = BIT(pin - 32);
        }
    }
}

/* Genesis 3/6 buttons */
static void IRAM_ATTR set_th_selection(uint8_t port) {
    uint16_t input = *(uint16_t *)wired_adapter.data[port].output;
    uint8_t value = 0;

    for (uint8_t i = 0, mask = 0x01; i < ARRAY_SIZE(gen_cycle_mask[0]); i++, mask <<= 1) {
        if ((gen_cycle_mask[sel[port]][i] & input) || gen_cycle_mask[sel[port]][i] == 0xFFFF) {
            value |= mask;
        }
    }

    for (uint8_t i = SIO_TR, mask = 0x2; mask; mask >>= 1, i++) {
        set_sio(port, i, value & mask);
    }
}

/* Saturn digital pad */
static void IRAM_ATTR set_th_tr_selection(uint8_t port) {
    uint8_t value = 0;

    switch (sel[port]) {
        case 0x0:
            value = wired_adapter.data[port].output[0] >> 4;
            break;
        case 0x1:
            value = wired_adapter.data[port].output[0] & 0xF;
            break;
        case 0x2:
            value = wired_adapter.data[port].output[1] >> 4;
            break;
        case 0x3:
            value = (wired_adapter.data[port].output[1] & 0xF);
            value &= 0xC;
            break;
    }
    tx_nibble(port, value);
}

/* Three-Wire Handshake */
static void IRAM_ATTR twh_tx(uint8_t port, uint8_t *data, uint32_t len) {
    uint32_t timeout;
    /* TX data */
    for (uint32_t i = 0; i < len; i++) {
        timeout = 0;
        while (GPIO.in & BIT(gpio_pin[port][SIO_TR]))
        {
            if (GPIO.in1.val & BIT(gpio_pin[port][SIO_TH] - 32) || timeout > TWH_TIMEOUT) {
                goto end;
            }
            timeout++;
        }
        tx_nibble(port, data[i] >> 4);
        set_sio(port, SIO_TL, 0);

        timeout = 0;
        while (!(GPIO.in & BIT(gpio_pin[port][SIO_TR])))
        {
            if (GPIO.in1.val & BIT(gpio_pin[port][SIO_TH] - 32) || timeout > TWH_TIMEOUT) {
                goto end;
            }
            timeout++;
        }
        tx_nibble(port, data[i] & 0xF);
        set_sio(port, SIO_TL, 1);
    }
end:
    ;
}

/* Saturn analog pad digital mode */
static void IRAM_ATTR set_analog_digital_pad(uint8_t port, uint8_t src_port) {
    buffer[0] = (ID2_SATURN_PAD << 4) | 2;
    memcpy(&buffer[1], wired_adapter.data[src_port].output, 2);
    buffer[3] = ID0_SATURN_THREEWIRE_HANDSHAKE >> 4;

    /* Set ID0 2nd nibble */
    tx_nibble(port, ID0_SATURN_THREEWIRE_HANDSHAKE & 0xF);

    twh_tx(port, buffer, 4);
    tx_nibble(port, ID0_SATURN_THREEWIRE_HANDSHAKE >> 4);
    set_sio(port, SIO_TL, 1);
}

/* Saturn analog pad */
static void IRAM_ATTR set_analog_pad(uint8_t port, uint8_t src_port) {
    buffer[0] = (ID2_SATURN_ANALOG_PAD << 4) | 6;
    memcpy(&buffer[1], wired_adapter.data[src_port].output, 6);
    buffer[7] = ID0_SATURN_THREEWIRE_HANDSHAKE >> 4;

    /* Set ID0 2nd nibble */
    tx_nibble(port, ID0_SATURN_THREEWIRE_HANDSHAKE & 0xF);

    twh_tx(port, buffer, 8);
    tx_nibble(port, ID0_SATURN_THREEWIRE_HANDSHAKE >> 4);
    set_sio(port, SIO_TL, 1);
}

/* Saturn multitap */
static void IRAM_ATTR set_saturn_multitap(uint8_t port, uint8_t first_port, uint8_t nb_port) {
    uint8_t *data = buffer;
    *data++ = (ID2_SATURN_MULTITAP << 4) | 1;
    *data++ = nb_port << 4;

    for (uint32_t i = 0, j = first_port; i < nb_port; i++, j++) {
        switch (mt_dev_type[port][i]) {
            case DEV_SATURN_DIGITAL:
            case DEV_SATURN_DIGITAL_TWH:
                *data++ = (ID2_SATURN_PAD << 4) | 2;
                memcpy(data, wired_adapter.data[j].output, 2);
                data += 2;
                break;
            case DEV_SATURN_ANALOG:
                *data++ = (ID2_SATURN_ANALOG_PAD << 4) | 6;
                memcpy(data, wired_adapter.data[j].output, 6);
                data += 6;
                break;
        }
    }

    *data++ = ID0_SATURN_THREEWIRE_HANDSHAKE >> 4;

    /* Set ID0 2nd nibble */
    tx_nibble(port, ID0_SATURN_THREEWIRE_HANDSHAKE & 0xF);

    twh_tx(port, buffer, data - buffer);
    tx_nibble(port, ID0_SATURN_THREEWIRE_HANDSHAKE >> 4);
    set_sio(port, SIO_TL, 1);
}

/* MegaDrive/Genesis multitap */
static void IRAM_ATTR set_gen_multitap(uint8_t port, uint8_t first_port, uint8_t nb_port) {
    uint8_t *data = buffer;
    uint32_t odd = 0;
    *data++ = 0x00;
    *data++ = (gen_ids[mt_dev_type[port][0]] << 4) | (gen_ids[mt_dev_type[port][1]] & 0xF);
    *data++ = (gen_ids[mt_dev_type[port][2]] << 4) | (gen_ids[mt_dev_type[port][3]] & 0xF);

    /* Set data */
    for (uint32_t i = 0, j = first_port; i < nb_port; i++, j++) {
        switch (mt_dev_type[port][i]) {
            case DEV_GENESIS_3BTNS:
                if (odd) {
                    *data++ &= (wired_adapter.data[j].output[24] >> 4) | 0xF0;
                    *data = (wired_adapter.data[j].output[24] << 4) | 0xF;
                }
                else {
                    *data++ = wired_adapter.data[j].output[24];
                }
                break;
            case DEV_GENESIS_6BTNS:
                if (odd) {
                    *data++ &= (wired_adapter.data[j].output[24] >> 4) | 0xF0;
                    *data = (wired_adapter.data[j].output[24] << 4) | 0xF;
                    *data++ &= (wired_adapter.data[j].output[25] >> 4) | 0xF0;
                    odd = 0;
                }
                else {
                    *data++ = wired_adapter.data[j].output[24];
                    *data = wired_adapter.data[j].output[25] | 0xF;
                    odd = 1;
                }
                break;
        }
    }
    if (odd) {
        data++;
    }

    twh_tx(port, buffer, data - buffer);
}

static void IRAM_ATTR sega_io_isr(void* arg) {
    static uint32_t last = 0;
    uint32_t cur = xthal_get_ccount();
    const uint32_t low_io = GPIO.acpu_int;
    const uint32_t high_io = GPIO.acpu_int1.intr;
    uint8_t port = 0;

    if (high_io & BIT(gpio_pin[0][SIO_TH] - 32)) {
        port = 0;
    }
    else if (high_io & BIT(gpio_pin[1][SIO_TH] - 32)) {
        port = 1;
    }
    else if (low_io & BIT(gpio_pin[0][SIO_TR])) {
        port = 0;
    }
    else if (low_io & BIT(gpio_pin[1][SIO_TR])) {
        port = 1;
    }

    switch (dev_type[port]) {
        case DEV_GENESIS_3BTNS:
            if (GPIO.in1.val & BIT(gpio_pin[port][SIO_TH] - 32)) {
                sel[port] = 1;
            }
            else {
                sel[port] = 0;
            }
            set_th_selection(port);
            break;
        case DEV_GENESIS_6BTNS:
            if (sel[port] > 0 && ((cur - last)/CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ < 100)) {
                sel[port]++;
            }
            else if (GPIO.in1.val & BIT(gpio_pin[port][SIO_TH] - 32)) {
                sel[port] = 1;
            }
            else {
                sel[port] = 0;
            }
            set_th_selection(port);
            break;
        case DEV_GENESIS_MULTITAP:
            break;
        case DEV_GENESIS_MOUSE:
            break;
        case DEV_SATURN_DIGITAL:
        {
            uint8_t tmp = 0;
            if (GPIO.in1.val & BIT(gpio_pin[port][SIO_TH] - 32)) {
                tmp |= 0x1;
            }
            if (GPIO.in & BIT(gpio_pin[port][SIO_TR])) {
                tmp |= 0x2;
            }
            sel[port] = tmp;
            set_th_tr_selection(port);
            break;
        }
        case DEV_SATURN_DIGITAL_TWH:
            if (!(GPIO.in1.val & BIT(gpio_pin[port][SIO_TH] - 32))) {
                set_analog_digital_pad(port, mt_first_port[port]);
            }
            break;
        case DEV_SATURN_ANALOG:
            if (!(GPIO.in1.val & BIT(gpio_pin[port][SIO_TH] - 32))) {
                set_analog_pad(port, mt_first_port[port]);
            }
            break;
        case DEV_SATURN_MULTITAP:
            if (!(GPIO.in1.val & BIT(gpio_pin[port][SIO_TH] - 32))) {
                set_saturn_multitap(port, mt_first_port[port], MT_PORT_MAX);
            }
            break;
        case DEV_SATURN_KB:
            break;
        case DEV_EA_MULTITAP:
            break;
    }

    last = cur;
    if (high_io) GPIO.status1_w1tc.intr_st = high_io;
    if (low_io) GPIO.status_w1tc = low_io;
}

static void IRAM_ATTR genesis_2p_isr(void* arg) {
    //static uint32_t last = 0;
    //uint32_t cur = xthal_get_ccount();
    uint32_t cycle = !(GPIO.in1.val & BIT(P1_TH_PIN - 32));
    uint32_t cycle2 = !(GPIO.in1.val & BIT(P2_TH_PIN - 32));

    GPIO.out = map1[cycle] & map2[cycle2];
    GPIO.out1.val = map1[cycle + 3] & map2[cycle2 + 3];

    if (GPIO.in1.val & BIT(P2_TH_PIN - 32)) {
        while (!(GPIO.in1.val & BIT(P1_TH_PIN - 32)));
        GPIO.out = map1[0] & map2[cycle2];
        GPIO.out1.val = map1[3] & map2[cycle2 + 3];
    }
    else {
        while (!(GPIO.in1.val & BIT(P2_TH_PIN - 32)));
        GPIO.out = map1[cycle] & map2[0];
        GPIO.out1.val = map1[cycle + 3] & map2[3];
    }

    //last = cur;
    const uint32_t low_io = GPIO.acpu_int;
    const uint32_t high_io = GPIO.acpu_int1.intr;
    if (high_io) GPIO.status1_w1tc.intr_st = high_io;
    if (low_io) GPIO.status_w1tc = low_io;
}

static void selection_refresh_task(void *arg) {
    uint32_t timeout, cur_in, prev_in, change, lock = 0;
    uint32_t p1_out0 = GPIO.out | ~P1_OUT0_MASK;
    uint32_t p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
    uint32_t p2_out0 = GPIO.out | ~P2_OUT0_MASK;
    uint32_t p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;

    while (1) {
        timeout = 0;
        cur_in = prev_in = GPIO.in1.val;
        while (!(change = cur_in ^ prev_in)) {
            prev_in = cur_in;
            cur_in = GPIO.in1.val;
        }

        if (change & BIT(P1_TH_PIN - 32)) {
p1_poll_start:
            if (cur_in & BIT(P1_TH_PIN - 32)) {
                goto p1_reverse_poll;
            }
            GPIO.out = map1[1] & p2_out0;                                /* P1 Cycle0 low */
            GPIO.out1.val = map1[4] & p2_out1;
            if (!lock) {
                DPORT_STALL_OTHER_CPU_START();
                ++lock;
            }
            p1_out0 = GPIO.out | ~P1_OUT0_MASK;
            p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
            if (dev_type[0] == DEV_GENESIS_MULTITAP) {
                GPIO.out1_w1ts.val = BIT(TP_CTRL_PIN - 32);
                if (lock) {
                    DPORT_STALL_OTHER_CPU_END();
                    lock = 0;
                }
                set_gen_multitap(0, mt_first_port[0], MT_GEN_PORT_MAX);
                if (GPIO.in1.val & BIT(P1_TH_PIN - 32)) {
                    GPIO.out = map1[0] & p2_out0;
                    GPIO.out1.val = map1[3] & p2_out1;
                    p1_out0 = GPIO.out | ~P1_OUT0_MASK;
                    p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
                    goto next_poll;
                }
            }
            timeout = 0;
            cur_in = prev_in = GPIO.in1.val;
            while (!(change = cur_in ^ prev_in)) {
                prev_in = cur_in;
                cur_in = GPIO.in1.val;
                if (++timeout > POLL_TIMEOUT) {
                    goto next_poll;
                }
            }
            if (change & BIT(P2_TH_PIN - 32)) {
                goto p2_poll_start;
            }
p1_reverse_poll:
            GPIO.out = map1[0] & p2_out0;                                /* P1 Cycle0 high */
            GPIO.out1.val = map1[3] & p2_out1;
            if (!lock) {
                DPORT_STALL_OTHER_CPU_START();
                ++lock;
            }
            p1_out0 = GPIO.out | ~P1_OUT0_MASK;
            p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
            timeout = 0;
            cur_in = prev_in = GPIO.in1.val;
            while (!(change = cur_in ^ prev_in)) {
                prev_in = cur_in;
                cur_in = GPIO.in1.val;
                if (++timeout > POLL_TIMEOUT) {
                    goto next_poll;
                }
            }
            if (change & BIT(P2_TH_PIN - 32)) {
                goto p2_poll_start;
            }
            GPIO.out = map1[1] & p2_out0;                                /* P1 Cycle1 low */
            GPIO.out1.val = map1[4] & p2_out1;
            p1_out0 = GPIO.out | ~P1_OUT0_MASK;
            p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
            if (dev_type[0] == DEV_GENESIS_6BTNS) {
                timeout = 0;
                cur_in = prev_in = GPIO.in1.val;
                while (!(change = cur_in ^ prev_in)) {
                    prev_in = cur_in;
                    cur_in = GPIO.in1.val;
                    if (++timeout > POLL_TIMEOUT) {
                        goto next_poll;
                    }
                }
                if (change & BIT(P2_TH_PIN - 32)) {
                    goto p2_poll_start;
                }
                GPIO.out = map1[0] & p2_out0;                            /* P1 Cycle1 high */
                GPIO.out1.val = map1[3] & p2_out1;
                p1_out0 = GPIO.out | ~P1_OUT0_MASK;
                p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
                timeout = 0;
                cur_in = prev_in = GPIO.in1.val;
                while (!(change = cur_in ^ prev_in)) {
                    prev_in = cur_in;
                    cur_in = GPIO.in1.val;
                    if (++timeout > POLL_TIMEOUT) {
                        goto next_poll;
                    }
                }
                if (change & BIT(P2_TH_PIN - 32)) {
                    goto p2_poll_start;
                }
                GPIO.out = (map1[1] & SIX_BTNS_P1_C2_LO_MASK) & p2_out0; /* P1 Cycle2 low */
                GPIO.out1.val = map1[4] & p2_out1;
                p1_out0 = GPIO.out | ~P1_OUT0_MASK;
                p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
                timeout = 0;
                cur_in = prev_in = GPIO.in1.val;
                while (!(change = cur_in ^ prev_in)) {
                    prev_in = cur_in;
                    cur_in = GPIO.in1.val;
                    if (++timeout > POLL_TIMEOUT) {
                        goto next_poll;
                    }
                }
                if (change & BIT(P2_TH_PIN - 32)) {
                    goto p2_poll_start;
                }
                GPIO.out = map1[2] & p2_out0;                            /* P1 Cycle2 high XYZM */
                GPIO.out1.val = map1[5] & p2_out1;
                p1_out0 = GPIO.out | ~P1_OUT0_MASK;
                p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
                timeout = 0;
                cur_in = prev_in = GPIO.in1.val;
                while (!(change = cur_in ^ prev_in)) {
                    prev_in = cur_in;
                    cur_in = GPIO.in1.val;
                    if (++timeout > POLL_TIMEOUT) {
                        goto next_poll;
                    }
                }
                if (change & BIT(P2_TH_PIN - 32)) {
                    goto p2_poll_start;
                }
                GPIO.out = (map1[1] | SIX_BTNS_P1_C3_LO_MASK) & p2_out0; /* P1 Cycle3 low */
                GPIO.out1.val = map1[4] & p2_out1;
                p1_out0 = GPIO.out | ~P1_OUT0_MASK;
                p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
                timeout = 0;
                cur_in = prev_in = GPIO.in1.val;
                while (!(change = cur_in ^ prev_in)) {
                    prev_in = cur_in;
                    cur_in = GPIO.in1.val;
                    if (++timeout > POLL_TIMEOUT) {
                        goto next_poll;
                    }
                }
                if (change & BIT(P2_TH_PIN - 32)) {
                    goto p2_poll_start;
                }
                GPIO.out = map1[0] & p2_out0;                            /* P1 Cycle3 high */
                GPIO.out1.val = map1[3] & p2_out1;
                p1_out0 = GPIO.out | ~P1_OUT0_MASK;
                p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
            }
        }
        else {
p2_poll_start:
            if (cur_in & BIT(P2_TH_PIN - 32)) {
                goto p2_reverse_poll;
            }
            GPIO.out = p1_out0 & map2[1];                                /* P2 Cycle0 low */
            GPIO.out1.val = p1_out1 & map2[4];
            if (!lock) {
                DPORT_STALL_OTHER_CPU_START();
                ++lock;
            }
            p2_out0 = GPIO.out | ~P2_OUT0_MASK;
            p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
            if (dev_type[1] == DEV_GENESIS_MULTITAP) {
                GPIO.out1_w1ts.val = BIT(TP_CTRL_PIN - 32);
                if (lock) {
                    DPORT_STALL_OTHER_CPU_END();
                    lock = 0;
                }
                set_gen_multitap(1, mt_first_port[1], MT_GEN_PORT_MAX);
                if (GPIO.in1.val & BIT(P2_TH_PIN - 32)) {
                    GPIO.out = p1_out0 & map2[0];
                    GPIO.out1.val = p1_out1 & map2[3];
                    p2_out0 = GPIO.out | ~P2_OUT0_MASK;
                    p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
                    goto next_poll;
                }
            }
            timeout = 0;
            cur_in = prev_in = GPIO.in1.val;
            while (!(change = cur_in ^ prev_in)) {
                prev_in = cur_in;
                cur_in = GPIO.in1.val;
                if (++timeout > POLL_TIMEOUT) {
                    goto next_poll;
                }
            }
            if (change & BIT(P1_TH_PIN - 32)) {
                goto p1_poll_start;
            }
p2_reverse_poll:
            GPIO.out = p1_out0 & map2[0];                                /* P2 Cycle0 high */
            GPIO.out1.val = p1_out1 & map2[3];
            if (!lock) {
                DPORT_STALL_OTHER_CPU_START();
                ++lock;
            }
            p2_out0 = GPIO.out | ~P2_OUT0_MASK;
            p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
            timeout = 0;
            cur_in = prev_in = GPIO.in1.val;
            while (!(change = cur_in ^ prev_in)) {
                prev_in = cur_in;
                cur_in = GPIO.in1.val;
                if (++timeout > POLL_TIMEOUT) {
                    goto next_poll;
                }
            }
            if (change & BIT(P1_TH_PIN - 32)) {
                goto p1_poll_start;
            }
            GPIO.out = p1_out0 & map2[1];                                /* P2 Cycle1 low */
            GPIO.out1.val = p1_out1 & map2[4];
            p2_out0 = GPIO.out | ~P2_OUT0_MASK;
            p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
            if (dev_type[1] == DEV_GENESIS_6BTNS) {
                timeout = 0;
                cur_in = prev_in = GPIO.in1.val;
                while (!(change = cur_in ^ prev_in)) {
                    prev_in = cur_in;
                    cur_in = GPIO.in1.val;
                    if (++timeout > POLL_TIMEOUT) {
                        goto next_poll;
                    }
                }
                if (change & BIT(P1_TH_PIN - 32)) {
                    goto p1_poll_start;
                }
                GPIO.out = p1_out0 & map2[0];                            /* P2 Cycle1 high */
                GPIO.out1.val = p1_out1 & map2[3];
                p2_out0 = GPIO.out | ~P2_OUT0_MASK;
                p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
                timeout = 0;
                cur_in = prev_in = GPIO.in1.val;
                while (!(change = cur_in ^ prev_in)) {
                    prev_in = cur_in;
                    cur_in = GPIO.in1.val;
                    if (++timeout > POLL_TIMEOUT) {
                        goto next_poll;
                    }
                }
                if (change & BIT(P1_TH_PIN - 32)) {
                    goto p1_poll_start;
                }
                GPIO.out = p1_out0 & (map2[1] & SIX_BTNS_P2_C2_LO_MASK); /* P2 Cycle2 low */
                GPIO.out1.val = p1_out1 & map2[4];
                p2_out0 = GPIO.out | ~P2_OUT0_MASK;
                p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
                timeout = 0;
                cur_in = prev_in = GPIO.in1.val;
                while (!(change = cur_in ^ prev_in)) {
                    prev_in = cur_in;
                    cur_in = GPIO.in1.val;
                    if (++timeout > POLL_TIMEOUT) {
                        goto next_poll;
                    }
                }
                if (change & BIT(P1_TH_PIN - 32)) {
                    goto p1_poll_start;
                }
                GPIO.out = p1_out0 & map2[2];                            /* P2 Cycle2 high XYZM */
                GPIO.out1.val = p1_out1 & map2[5];
                p2_out0 = GPIO.out | ~P2_OUT0_MASK;
                p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
                timeout = 0;
                cur_in = prev_in = GPIO.in1.val;
                while (!(change = cur_in ^ prev_in)) {
                    prev_in = cur_in;
                    cur_in = GPIO.in1.val;
                    if (++timeout > POLL_TIMEOUT) {
                        goto next_poll;
                    }
                }
                if (change & BIT(P1_TH_PIN - 32)) {
                    goto p1_poll_start;
                }
                GPIO.out = p1_out0 & (map2[1] | SIX_BTNS_P2_C3_LO_MASK); /* P2 Cycle3 low */
                GPIO.out1.val = p1_out1 & map2[4];
                p2_out0 = GPIO.out | ~P2_OUT0_MASK;
                p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
                timeout = 0;
                cur_in = prev_in = GPIO.in1.val;
                while (!(change = cur_in ^ prev_in)) {
                    prev_in = cur_in;
                    cur_in = GPIO.in1.val;
                    if (++timeout > POLL_TIMEOUT) {
                        goto next_poll;
                    }
                }
                if (change & BIT(P1_TH_PIN - 32)) {
                    goto p1_poll_start;
                }
                GPIO.out = p1_out0 & map2[0];                            /* P2 Cycle3 high */
                GPIO.out1.val = p1_out1 & map2[3];
                p2_out0 = GPIO.out | ~P2_OUT0_MASK;
                p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
            }
//p2_next_poll:
            //p2_out0 = GPIO.out | ~P2_OUT0_MASK;
            //p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
        }
next_poll:
        if (lock) {
            DPORT_STALL_OTHER_CPU_END();
            lock = 0;
        }
    }
}

void sega_io_init(void)
{
    gpio_config_t io_conf = {0};
    uint8_t port_cnt = 0;
    uint32_t start_thread = 0;

    if (wired_adapter.system_id == SATURN) {
        switch (config.global_cfg.multitap_cfg) {
            case MT_SLOT_1:
                dev_type[0] = DEV_SATURN_MULTITAP;
                mt_first_port[1] = MT_PORT_MAX;
                break;
            case MT_SLOT_2:
                dev_type[1] = DEV_SATURN_MULTITAP;
                mt_first_port[1] = 1;
                break;
            case MT_DUAL:
                dev_type[0] = DEV_SATURN_MULTITAP;
                dev_type[1] = DEV_SATURN_MULTITAP;
                mt_first_port[1] = MT_PORT_MAX;
                break;
            default:
                mt_first_port[1] = 1;
        }

        for (uint32_t i = 0; i < ARRAY_SIZE(gpio_pin); i++) {
            if (dev_type[i] == DEV_SATURN_MULTITAP) {
                for (uint32_t j = 0; j < MT_PORT_MAX; j++) {
                    switch (config.out_cfg[port_cnt++].dev_mode) {
                        case DEV_PAD:
                            mt_dev_type[i][j] = DEV_SATURN_DIGITAL_TWH;
                            break;
                        case DEV_PAD_ALT:
                            mt_dev_type[i][j] = DEV_SATURN_ANALOG;
                            break;
                        case DEV_KB:
                            mt_dev_type[i][j] = DEV_SATURN_KB;
                            break;
                        case DEV_MOUSE:
                            mt_dev_type[i][j] = DEV_GENESIS_MOUSE;
                            break;
                    }
                }
            }
            else if (dev_type[i] == DEV_NONE) {
                switch (config.out_cfg[port_cnt++].dev_mode) {
                    case DEV_PAD:
                        dev_type[i] = DEV_SATURN_DIGITAL_TWH;
                        break;
                    case DEV_PAD_ALT:
                        dev_type[i] = DEV_SATURN_ANALOG;
                        break;
                    case DEV_KB:
                        dev_type[i] = DEV_SATURN_KB;
                        break;
                    case DEV_MOUSE:
                        dev_type[i] = DEV_GENESIS_MOUSE;
                        break;
                }
            }
        }
    }
    else {
        io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pin_bit_mask = 1ULL << TP_CTRL_PIN;
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = 1ULL << EA_CTRL_PIN;
        gpio_config(&io_conf);
        gpio_set_level(TP_CTRL_PIN, 0);
        gpio_set_level(EA_CTRL_PIN, 0);

        switch (config.global_cfg.multitap_cfg) {
            case MT_SLOT_1:
                dev_type[0] = DEV_GENESIS_MULTITAP;
                mt_first_port[1] = MT_GEN_PORT_MAX;
                break;
            case MT_SLOT_2:
                dev_type[1] = DEV_GENESIS_MULTITAP;
                mt_first_port[1] = 1;
                break;
            case MT_DUAL:
                dev_type[0] = DEV_GENESIS_MULTITAP;
                dev_type[1] = DEV_GENESIS_MULTITAP;
                mt_first_port[1] = MT_GEN_PORT_MAX;
                break;
            case MT_ALT:
                dev_type[0] = DEV_EA_MULTITAP;
                dev_type[1] = DEV_EA_MULTITAP;
                break;
            default:
                mt_first_port[1] = 1;
        }

        for (uint32_t i = 0; i < ARRAY_SIZE(gpio_pin); i++) {
            if (dev_type[i] == DEV_GENESIS_MULTITAP) {
                for (uint32_t j = 0; j < MT_GEN_PORT_MAX; j++) {
                    switch (config.out_cfg[port_cnt++].dev_mode) {
                        case DEV_PAD:
                            mt_dev_type[i][j] = DEV_GENESIS_3BTNS;
                            break;
                        case DEV_PAD_ALT:
                            mt_dev_type[i][j] = DEV_GENESIS_6BTNS;
                            break;
                        case DEV_MOUSE:
                            mt_dev_type[i][j] = DEV_GENESIS_MOUSE;
                            break;
                    }
                }
            }
            else if (dev_type[i] == DEV_NONE) {
                switch (config.out_cfg[port_cnt++].dev_mode) {
                    case DEV_PAD:
                        dev_type[i] = DEV_GENESIS_3BTNS;
                        break;
                    case DEV_PAD_ALT:
                        dev_type[i] = DEV_GENESIS_6BTNS;
                        break;
                    case DEV_MOUSE:
                        dev_type[i] = DEV_GENESIS_MOUSE;
                        break;
                }
            }
        }
    }

    /* TH */
    for (uint32_t i = 0; i < ARRAY_SIZE(gpio_pin); i++) {
        //if (dev_type[i] == DEV_SATURN_ANALOG) {
            io_conf.intr_type = GPIO_PIN_INTR_NEGEDGE;
        //}
        //else {
        //    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
        //}
        io_conf.pin_bit_mask = 1ULL << gpio_pin[i][SIO_TH];
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    }

    /* TR */
    for (uint32_t i = 0; i < ARRAY_SIZE(gpio_pin); i++) {
        if (dev_type[i] == DEV_SATURN_DIGITAL) {
            io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
        }
        else {
            io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
        }
        io_conf.pin_bit_mask = 1ULL << gpio_pin[i][SIO_TR];
        if (dev_type[i] == DEV_GENESIS_3BTNS || dev_type[i] == DEV_GENESIS_6BTNS) {
            io_conf.mode = GPIO_MODE_OUTPUT;
        }
        else {
            io_conf.mode = GPIO_MODE_INPUT;
        }
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
        if (dev_type[i] == DEV_GENESIS_3BTNS || dev_type[i] == DEV_GENESIS_6BTNS) {
            set_sio(i, SIO_TR, 1);
        }
    }

    /* TL, R, L, D, U */
    for (uint32_t i = 0; i < ARRAY_SIZE(gpio_pin); i++) {
        for (uint32_t j = SIO_TL; j <= SIO_U; j++) {
            io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
            io_conf.pin_bit_mask = 1ULL << gpio_pin[i][j];
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            gpio_config(&io_conf);
            set_sio(i, j, 1);
        }
    }

    /* Init half ID0 */
    for (uint32_t i = 0; i < ARRAY_SIZE(gpio_pin); i++) {
        switch (dev_type[i]) {
            case DEV_GENESIS_3BTNS:
            case DEV_GENESIS_6BTNS:
                if (GPIO.in1.val & BIT(gpio_pin[i][SIO_TH] - 32)) {
                    sel[i] = 1;
                }
                else {
                    sel[i] = 0;
                }
                set_th_selection(i);
                start_thread = 1;
                break;
            case DEV_GENESIS_MULTITAP:
                start_thread = 1;
                break;
            case DEV_GENESIS_MOUSE:
                break;
            case DEV_SATURN_DIGITAL:
            {
                uint8_t tmp = 0;
                if (GPIO.in1.val & BIT(gpio_pin[i][SIO_TH] - 32)) {
                    tmp |= 0x1;
                }
                if (GPIO.in & BIT(gpio_pin[i][SIO_TR])) {
                    tmp |= 0x2;
                }
                sel[i] = tmp;
                set_th_tr_selection(i);
                break;
            }
            case DEV_SATURN_DIGITAL_TWH:
            case DEV_SATURN_ANALOG:
            case DEV_SATURN_MULTITAP:
            case DEV_SATURN_KB:
                tx_nibble(i, ID0_SATURN_THREEWIRE_HANDSHAKE >> 4);
                break;
            case DEV_EA_MULTITAP:
                break;
        }
    }

    if (start_thread) {
        xTaskCreatePinnedToCore(selection_refresh_task, "selection_refresh_task", 2048, NULL, 10, NULL, 1);
    }
    else {
        esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3, sega_io_isr, NULL, NULL);
    }
}
