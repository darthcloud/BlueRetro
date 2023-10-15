/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp32/rom/ets_sys.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "adapter/kb_monitor.h"
#include "adapter/wired/genesis.h"
#include "adapter/wired/saturn.h"
#include "system/core0_stall.h"
#include "system/delay.h"
#include "system/gpio.h"
#include "system/intr.h"
#include "sega_io.h"

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
#define ID0_SATURN_PAD 0xC0
#define ID0_SATURN_THREEWIRE_HANDSHAKE 0x11
#define ID0_SATURN_CLOCKED_SERIAL 0x22
#define ID0_SATURN_CLOCKED_PARALLEL 0x33

#define P1_MOUSE_ID0_HI 0xFF79FFD5
#define P1_MOUSE_ID0_LO 0xFFF9FFFD
#define P2_MOUSE_ID0_HI 0xFD95FFFD
#define P2_MOUSE_ID0_LO 0xFFBDFFFD
#define P1_SAT_TWH_ID0_LO_HI 0xFF79FFDD
#define P2_SAT_TWH_ID0_LO_HI 0xFD9DFFFD
#define P1_SAT_PARA_ID0_LO 0xFFFDFFD5
#define P1_SAT_PARA_ID0_HI 0xFFFDFFFD
#define P2_SAT_PARA_ID0_LO 0xFFD5FFFD
#define P2_SAT_PARA_ID0_HI 0xFFFDFFFD

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
#define ID2_LEGACY 0xE
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
    DEV_SEGA_MOUSE,
    DEV_GENESIS_XBAND_KB,
    DEV_SATURN_DIGITAL,
    DEV_SATURN_DIGITAL_TWH,
    DEV_SATURN_ANALOG,
    DEV_SATURN_MULTITAP,
    DEV_SATURN_KB,
    DEV_EA_MULTITAP,
    DEV_TYPE_MAX,
};

static const uint8_t gen_ids[DEV_TYPE_MAX] = {
    0xF, 0x0, 0x1, 0xF, 0x2, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF,
};

static const uint8_t gpio_pin[2][7] = {
    {35, 27, 26, 23, 18,  5,  3},
    {36, 16, 33, 25, 22, 21, 19},
};

static uint32_t id0_lo[2];
static uint32_t id0_hi[2];
static uint8_t dev_type[2] = {0};
static uint8_t mt_dev_type[2][MT_PORT_MAX] = {0};
static uint8_t mt_first_port[2] = {0};
static uint8_t buffer[6*6];
static uint32_t *map1 = wired_adapter.data[0].output32;
static uint32_t *map2 = wired_adapter.data[1].output32;
static uint32_t *map1_mask = wired_adapter.data[0].output_mask32;
static uint32_t *map2_mask = wired_adapter.data[1].output_mask32;

static inline void load_mouse_axes(uint8_t port, uint8_t *flags, uint8_t *axes) {
    uint8_t *relative = NULL;
    int32_t *raw_axes = NULL;
    int32_t val = 0;
    uint8_t sign_mask = 0;

    if (wired_adapter.system_id == GENESIS) {
        relative = (uint8_t *)(wired_adapter.data[port].output + 26);
        raw_axes = (int32_t *)(wired_adapter.data[port].output + 28);
    }
    else {
        relative = (uint8_t *)(wired_adapter.data[port].output + 2);
        raw_axes = (int32_t *)(wired_adapter.data[port].output + 4);
    }

    for (uint32_t i = 0; i < 2; i++) {
        if (i & 0x01) {
            sign_mask = 0x20;
        }
        else {
            sign_mask = 0x10;
        }

        if (relative[i]) {
            val = atomic_clear(&raw_axes[i]);
        }
        else {
            val = raw_axes[i];
        }

        if (val > 255) {
            axes[i] = 0xFF;
            *flags &= ~sign_mask;
        }
        else if (val < -256) {
            axes[i] = 0x00;
            *flags |= sign_mask;
        }
        else {
            axes[i] = (uint8_t)val;
            if (val < 0) {
                *flags |= sign_mask;
            }
            else {
                *flags &= ~sign_mask;
            }
        }
    }
}

static void tx_nibble(uint8_t port, uint8_t data) {
    for (uint8_t i = SIO_R, mask = 0x8; mask; mask >>= 1, i++) {
        if (data & mask) {
            GPIO.out_w1ts = BIT(gpio_pin[port][i]);
        }
        else {
            GPIO.out_w1tc = BIT(gpio_pin[port][i]);
        }
    }
}

static void set_sio(uint8_t port, uint8_t sio, uint8_t value) {
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

/* Three-Wire Handshake */
static void twh_tx(uint8_t port, uint8_t *data, uint32_t len, uint8_t ack_delay) {
    uint32_t timeout = 0;
    uint8_t tl_state = 0;
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
        delay_us(ack_delay);
        set_sio(port, SIO_TL, tl_state);
        tl_state ^= 0x01;

        timeout = 0;
        while (!(GPIO.in & BIT(gpio_pin[port][SIO_TR])))
        {
            if (GPIO.in1.val & BIT(gpio_pin[port][SIO_TH] - 32) || timeout > TWH_TIMEOUT) {
                goto end;
            }
            timeout++;
        }
        tx_nibble(port, data[i] & 0xF);
        delay_us(ack_delay);
        set_sio(port, SIO_TL, tl_state);
        tl_state ^= 0x01;
    }

    /* Answer extra cycle with last byte */
    while (1) {
        timeout = 0;
        while (GPIO.in & BIT(gpio_pin[port][SIO_TR]))
        {
            if (GPIO.in1.val & BIT(gpio_pin[port][SIO_TH] - 32) || timeout > TWH_TIMEOUT) {
                goto end;
            }
            timeout++;
        }
        delay_us(ack_delay);
        set_sio(port, SIO_TL, tl_state);
        tl_state ^= 0x01;

        timeout = 0;
        while (!(GPIO.in & BIT(gpio_pin[port][SIO_TR])))
        {
            if (GPIO.in1.val & BIT(gpio_pin[port][SIO_TH] - 32) || timeout > TWH_TIMEOUT) {
                goto end;
            }
            timeout++;
        }
        delay_us(ack_delay);
        set_sio(port, SIO_TL, tl_state);
        tl_state ^= 0x01;
    }
end:
    ;
}

/* Saturn analog pad digital mode */
static void set_analog_digital_pad(uint8_t port, uint8_t src_port) {
    buffer[0] = (ID2_SATURN_PAD << 4) | 2;
    buffer[1] = wired_adapter.data[src_port].output[0] | wired_adapter.data[src_port].output_mask[0];
    buffer[2] = wired_adapter.data[src_port].output[1] | wired_adapter.data[src_port].output_mask[1];
    buffer[3] = ID0_SATURN_THREEWIRE_HANDSHAKE >> 4;

    twh_tx(port, buffer, 4, 0);
    ++wired_adapter.data[src_port].frame_cnt;
    saturn_gen_turbo_mask(&wired_adapter.data[src_port]);
}

/* Saturn analog pad */
static void set_analog_pad(uint8_t port, uint8_t src_port) {
    buffer[0] = (ID2_SATURN_ANALOG_PAD << 4) | 6;
    buffer[1] = wired_adapter.data[src_port].output[0] | wired_adapter.data[src_port].output_mask[0];
    buffer[2] = wired_adapter.data[src_port].output[1] | wired_adapter.data[src_port].output_mask[1];
    for (uint32_t i = 3; i < 5; ++i) {
        buffer[i] = (wired_adapter.data[src_port].output_mask[i - 1]) ?
            wired_adapter.data[src_port].output_mask[i - 1] : wired_adapter.data[src_port].output[i - 1];
    }
    buffer[5] = wired_adapter.data[src_port].output[4] & wired_adapter.data[src_port].output_mask[4];
    buffer[6] = wired_adapter.data[src_port].output[5] & wired_adapter.data[src_port].output_mask[5];
    buffer[7] = ID0_SATURN_THREEWIRE_HANDSHAKE >> 4;

    twh_tx(port, buffer, 8, 0);
    ++wired_adapter.data[src_port].frame_cnt;
    saturn_gen_turbo_mask(&wired_adapter.data[src_port]);
}

/* SEGA Mouse */
static void set_sega_mouse(uint8_t port, uint8_t src_port) {
    buffer[0] = 0xFF;
    if (wired_adapter.system_id == GENESIS) {
        memcpy(&buffer[1], &wired_adapter.data[src_port].output[24], 1);
        load_mouse_axes(port, &buffer[1], &buffer[2]);
    }
    else {
        memcpy(&buffer[1], wired_adapter.data[src_port].output, 1);
        load_mouse_axes(port, &buffer[1], &buffer[2]);
    }
    buffer[4] = ID0_MOUSE >> 4;

    twh_tx(port, buffer, 5, 14);
}

/* Saturn keyboard */
static void set_saturn_keyboard(uint8_t port, uint8_t src_port) {
    uint32_t len;
    buffer[0] = (ID2_SATURN_KB << 4) | 4;
    memcpy(&buffer[1], wired_adapter.data[src_port].output, 2);
    if (kbmon_get_code(src_port, &buffer[3], &len)) {
        buffer[3] = 0x06;
        buffer[4] = 0x00;
    }
    buffer[5] = ID0_SATURN_THREEWIRE_HANDSHAKE >> 4;

    twh_tx(port, buffer, 6, 0);
}

/* Saturn multitap */
static void set_saturn_multitap(uint8_t port, uint8_t first_port, uint8_t nb_port) {
    uint8_t *data = buffer;
    uint32_t len;
    *data++ = (ID2_SATURN_MULTITAP << 4) | 1;
    *data++ = nb_port << 4;

    for (uint32_t i = 0, j = first_port; i < nb_port; i++, j++) {
        switch (mt_dev_type[port][i]) {
            case DEV_SATURN_DIGITAL:
            case DEV_SATURN_DIGITAL_TWH:
                *data++ = (ID2_SATURN_PAD << 4) | 2;
                *data++ = wired_adapter.data[j].output[0] | wired_adapter.data[j].output_mask[0];
                *data++ = wired_adapter.data[j].output[1] | wired_adapter.data[j].output_mask[1];
                ++wired_adapter.data[j].frame_cnt;
                saturn_gen_turbo_mask(&wired_adapter.data[j]);
                break;
            case DEV_SATURN_ANALOG:
                *data++ = (ID2_SATURN_ANALOG_PAD << 4) | 6;
                *data++ = wired_adapter.data[j].output[0] | wired_adapter.data[j].output_mask[0];
                *data++ = wired_adapter.data[j].output[1] | wired_adapter.data[j].output_mask[1];
                for (uint32_t k = 2; k < 4; ++k) {
                    *data++ = (wired_adapter.data[j].output_mask[k]) ?
                        wired_adapter.data[j].output_mask[k] : wired_adapter.data[j].output[k];
                }
                *data++ = wired_adapter.data[j].output[4] & wired_adapter.data[j].output_mask[4];
                *data++ = wired_adapter.data[j].output[5] & wired_adapter.data[j].output_mask[5];
                ++wired_adapter.data[j].frame_cnt;
                saturn_gen_turbo_mask(&wired_adapter.data[j]);
                break;
            case DEV_SATURN_KB:
                *data++ = (ID2_SATURN_KB << 4) | 4;
                memcpy(data, wired_adapter.data[j].output, 2);
                data += 2;
                if (kbmon_get_code(j, data, &len)) {
                    buffer[3] = 0x06;
                    buffer[4] = 0x00;
                }
                data += len;
                break;
            case DEV_SEGA_MOUSE:
                *data++ = (ID2_LEGACY << 4) | 3;
                memcpy(data, wired_adapter.data[j].output, 1);
                load_mouse_axes(j, data, &data[1]);
                data += 3;
                break;
        }
    }

    *data++ = ID0_SATURN_THREEWIRE_HANDSHAKE >> 4;

    twh_tx(port, buffer, data - buffer, 0);
}

/* MegaDrive/Genesis multitap */
static void set_gen_multitap(uint8_t port, uint8_t first_port, uint8_t nb_port) {
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
                    uint8_t tmp = wired_adapter.data[j].output[24] | wired_adapter.data[j].output_mask[24];
                    *data++ &= (tmp >> 4) | 0xF0;
                    *data = (tmp << 4) | 0xF;
                }
                else {
                    *data++ = wired_adapter.data[j].output[24] | wired_adapter.data[j].output_mask[24];
                }
                ++wired_adapter.data[j].frame_cnt;
                genesis_twh_gen_turbo_mask(&wired_adapter.data[j]);
                break;
            case DEV_GENESIS_6BTNS:
                if (odd) {
                    uint8_t tmp = wired_adapter.data[j].output[24] | wired_adapter.data[j].output_mask[24];
                    *data++ &= (tmp >> 4) | 0xF0;
                    *data = (tmp << 4) | 0xF;
                    tmp = wired_adapter.data[j].output[25] | wired_adapter.data[j].output_mask[25];
                    *data++ &= (tmp >> 4) | 0xF0;
                    odd = 0;
                }
                else {
                    *data++ = wired_adapter.data[j].output[24] | wired_adapter.data[j].output_mask[24];
                    *data = (wired_adapter.data[j].output[25] | wired_adapter.data[j].output_mask[25]) | 0xF;
                    odd = 1;
                }
                ++wired_adapter.data[j].frame_cnt;
                genesis_twh_gen_turbo_mask(&wired_adapter.data[j]);
                break;
            case DEV_SEGA_MOUSE:
                if (odd) {
                    uint8_t tmp[3] = {0};
                    tmp[0] = wired_adapter.data[j].output[24];
                    load_mouse_axes(j, &tmp[0], &tmp[1]);

                    *data++ &= (tmp[0] >> 4) | 0xF0;
                    *data = (tmp[0] << 4) | 0xF;
                    *data++ &= (tmp[1] >> 4) | 0xF0;
                    *data = (tmp[1] << 4) | 0xF;
                    *data++ &= (tmp[2] >> 4) | 0xF0;
                    *data = (tmp[2] << 4) | 0xF;
                }
                else {
                    *data = wired_adapter.data[j].output[24];
                    load_mouse_axes(j, data, &data[1]);
                    data += 3;
                }
                break;
        }
    }
    if (odd) {
        data++;
    }

    twh_tx(port, buffer, data - buffer, 0);
}

static void sega_genesis_task(void) {
    uint32_t timeout, cur_in, prev_in, change, idx = 0, lock = 0;
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
            GPIO.out = (map1[1] | map1_mask[1]) & p2_out0;                                /* P1 Cycle0 low */
            GPIO.out1.val = (map1[4] | map1_mask[4]) & p2_out1;
            if (!lock) {
                core0_stall_start();
                ++lock;
            }
            p1_out0 = GPIO.out | ~P1_OUT0_MASK;
            p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
            idx = 0;
            if (dev_type[0] == DEV_GENESIS_MULTITAP) {
                GPIO.out1_w1ts.val = BIT(TP_CTRL_PIN - 32);
                if (lock) {
                    core0_stall_end();
                    lock = 0;
                }
                set_gen_multitap(0, mt_first_port[0], MT_GEN_PORT_MAX);
                if (GPIO.in1.val & BIT(P1_TH_PIN - 32)) {
                    GPIO.out = (map1[0] | map1_mask[0]) & p2_out0;
                    GPIO.out1.val = (map1[3] | map1_mask[3]) & p2_out1;
                    p1_out0 = GPIO.out | ~P1_OUT0_MASK;
                    p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
                    goto next_poll;
                }
            }
            else if (dev_type[0] == DEV_SEGA_MOUSE) {
                GPIO.out1_w1ts.val = BIT(TP_CTRL_PIN - 32);
                if (lock) {
                    core0_stall_end();
                    lock = 0;
                }
                set_sega_mouse(0, mt_first_port[0]);
                if (GPIO.in1.val & BIT(P1_TH_PIN - 32)) {
                    GPIO.out = (map1[0] | map1_mask[0]) & p2_out0;
                    GPIO.out1.val = (map1[3] | map1_mask[3]) & p2_out1;
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
            GPIO.out = (map1[0] | map1_mask[0]) & p2_out0;                                /* P1 Cycle0 high */
            GPIO.out1.val = (map1[3] | map1_mask[3]) & p2_out1;
            if (!lock) {
                core0_stall_start();
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
            GPIO.out = (map1[1] | map1_mask[1]) & p2_out0;                                /* P1 Cycle1 low */
            GPIO.out1.val = (map1[4] | map1_mask[4]) & p2_out1;
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
                GPIO.out = (map1[0] | map1_mask[0]) & p2_out0;                            /* P1 Cycle1 high */
                GPIO.out1.val = (map1[3] | map1_mask[3]) & p2_out1;
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
                GPIO.out = ((map1[1] | map1_mask[1]) & SIX_BTNS_P1_C2_LO_MASK) & p2_out0; /* P1 Cycle2 low */
                GPIO.out1.val = (map1[4] | map1_mask[4]) & p2_out1;
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
                GPIO.out = (map1[2] | map1_mask[2]) & p2_out0;                            /* P1 Cycle2 high XYZM */
                GPIO.out1.val = (map1[5] | map1_mask[5]) & p2_out1;
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
                GPIO.out = ((map1[1] | map1_mask[1]) | SIX_BTNS_P1_C3_LO_MASK) & p2_out0; /* P1 Cycle3 low */
                GPIO.out1.val = (map1[4] | map1_mask[4]) & p2_out1;
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
                GPIO.out = (map1[0] | map1_mask[0]) & p2_out0;                            /* P1 Cycle3 high */
                GPIO.out1.val = (map1[3] | map1_mask[3]) & p2_out1;
                p1_out0 = GPIO.out | ~P1_OUT0_MASK;
                p1_out1 = GPIO.out1.val | ~P1_OUT1_MASK;
            }
        }
        else {
p2_poll_start:
            if (cur_in & BIT(P2_TH_PIN - 32)) {
                goto p2_reverse_poll;
            }
            GPIO.out = p1_out0 & (map2[1] | map2_mask[1]);                                /* P2 Cycle0 low */
            GPIO.out1.val = p1_out1 & (map2[4] | map2_mask[4]);
            if (!lock) {
                core0_stall_start();
                ++lock;
            }
            p2_out0 = GPIO.out | ~P2_OUT0_MASK;
            p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
            idx = 1;
            if (dev_type[1] == DEV_GENESIS_MULTITAP) {
                GPIO.out1_w1ts.val = BIT(TP_CTRL_PIN - 32);
                if (lock) {
                    core0_stall_end();
                    lock = 0;
                }
                set_gen_multitap(1, mt_first_port[1], MT_GEN_PORT_MAX);
                if (GPIO.in1.val & BIT(P2_TH_PIN - 32)) {
                    GPIO.out = p1_out0 & (map2[0] | map2_mask[0]);
                    GPIO.out1.val = p1_out1 & (map2[3] | map2_mask[3]);
                    p2_out0 = GPIO.out | ~P2_OUT0_MASK;
                    p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
                    goto next_poll;
                }
            }
            else if (dev_type[1] == DEV_SEGA_MOUSE) {
                GPIO.out1_w1ts.val = BIT(TP_CTRL_PIN - 32);
                if (lock) {
                    core0_stall_end();
                    lock = 0;
                }
                set_sega_mouse(1, mt_first_port[1]);
                if (GPIO.in1.val & BIT(P2_TH_PIN - 32)) {
                    GPIO.out = p1_out0 & (map2[0] | map2_mask[0]);
                    GPIO.out1.val = p1_out1 & (map2[3] | map2_mask[3]);
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
            GPIO.out = p1_out0 & (map2[0] | map2_mask[0]);                                /* P2 Cycle0 high */
            GPIO.out1.val = p1_out1 & (map2[3] | map2_mask[3]);
            if (!lock) {
                core0_stall_start();
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
            GPIO.out = p1_out0 & (map2[1] | map2_mask[1]);                                /* P2 Cycle1 low */
            GPIO.out1.val = p1_out1 & (map2[4] | map2_mask[4]);
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
                GPIO.out = p1_out0 & (map2[0] | map2_mask[0]);                            /* P2 Cycle1 high */
                GPIO.out1.val = p1_out1 & (map2[3] | map2_mask[3]);
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
                GPIO.out = p1_out0 & ((map2[1] | map2_mask[1]) & SIX_BTNS_P2_C2_LO_MASK); /* P2 Cycle2 low */
                GPIO.out1.val = p1_out1 & (map2[4] | map2_mask[4]);
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
                GPIO.out = p1_out0 & (map2[2] | map2_mask[2]);                            /* P2 Cycle2 high XYZM */
                GPIO.out1.val = p1_out1 & (map2[5] | map2_mask[5]);
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
                GPIO.out = p1_out0 & ((map2[1] | map2_mask[1]) | SIX_BTNS_P2_C3_LO_MASK); /* P2 Cycle3 low */
                GPIO.out1.val = p1_out1 & (map2[4] | map2_mask[4]);
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
                GPIO.out = p1_out0 & (map2[0] | map2_mask[0]);                            /* P2 Cycle3 high */
                GPIO.out1.val = p1_out1 & (map2[3] | map2_mask[3]);
                p2_out0 = GPIO.out | ~P2_OUT0_MASK;
                p2_out1 = GPIO.out1.val | ~P2_OUT1_MASK;
            }
        }
next_poll:
        if (lock) {
            core0_stall_end();
            lock = 0;
            ++wired_adapter.data[idx].frame_cnt;
            genesis_gen_turbo_mask(idx, &wired_adapter.data[idx]);
        }
    }
}

static void sega_saturn_task(void) {
    uint32_t timeout, cur_in, prev_in, change;
    uint32_t p1_out0 = GPIO.out | ~P1_OUT0_MASK;
    uint32_t p2_out0 = GPIO.out | ~P2_OUT0_MASK;

    while (1) {
        timeout = 0;
        cur_in = prev_in = GPIO.in1.val;
        while (!(change = cur_in ^ prev_in)) {
            prev_in = cur_in;
            cur_in = GPIO.in1.val;
        }

        if (change & BIT(P1_TH_PIN - 32)) {
            GPIO.out = id0_lo[0] & p2_out0;
            p1_out0 = GPIO.out | ~P1_OUT0_MASK;
            switch (dev_type[0]) {
                case DEV_SATURN_DIGITAL_TWH:
                    set_analog_digital_pad(0, mt_first_port[0]);
                    break;
                case DEV_SATURN_ANALOG:
                    set_analog_pad(0, mt_first_port[0]);
                    break;
                case DEV_SATURN_MULTITAP:
                    set_saturn_multitap(0, mt_first_port[0], MT_PORT_MAX);
                    break;
                case DEV_SATURN_KB:
                    set_saturn_keyboard(0, mt_first_port[0]);
                    break;
                case DEV_SEGA_MOUSE:
                    set_sega_mouse(0, mt_first_port[0]);
                    break;
                default:
                    ets_printf("BADTYPE%s\n", dev_type[0]);
            }
            if (GPIO.in1.val & BIT(P1_TH_PIN - 32)) {
                goto p1_set_id0_hi;
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
p1_set_id0_hi:
            GPIO.out = id0_hi[0] & p2_out0;
            p1_out0 = GPIO.out | ~P1_OUT0_MASK;
        }
        else {
            GPIO.out = p1_out0 & id0_lo[1];
            p2_out0 = GPIO.out | ~P2_OUT0_MASK;
            switch (dev_type[1]) {
                case DEV_SATURN_DIGITAL_TWH:
                    set_analog_digital_pad(1, mt_first_port[1]);
                    break;
                case DEV_SATURN_ANALOG:
                    set_analog_pad(1, mt_first_port[1]);
                    break;
                case DEV_SATURN_MULTITAP:
                    set_saturn_multitap(1, mt_first_port[1], MT_PORT_MAX);
                    break;
                case DEV_SATURN_KB:
                    set_saturn_keyboard(1, mt_first_port[1]);
                    break;
                case DEV_SEGA_MOUSE:
                    set_sega_mouse(1, mt_first_port[1]);
                    break;
                default:
                    ets_printf("BADTYPE%s\n", dev_type[1]);
            }
            if (GPIO.in1.val & BIT(P2_TH_PIN - 32)) {
                goto p2_set_id0_hi;
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
p2_set_id0_hi:
            GPIO.out = p1_out0 & id0_hi[1];
            p2_out0 = GPIO.out | ~P2_OUT0_MASK;
        }
next_poll:
        ;
    }
}

static void ea_genesis_task(void) {
    uint32_t cur_in0, prev_in0, change0 = 0, cur_in1, prev_in1, change1 = 1, id = 0;

    cur_in0 = GPIO.in;
    cur_in1 = GPIO.in1.val;
    while (1) {
        prev_in0 = cur_in0;
        prev_in1 = cur_in1;
        cur_in0 = GPIO.in;
        cur_in1 = GPIO.in1.val;
        while (!(change0 = cur_in0 ^ prev_in0) && !(change1 = cur_in1 ^ prev_in1)) {
            prev_in0 = cur_in0;
            prev_in1 = cur_in1;
            cur_in0 = GPIO.in;
            cur_in1 = GPIO.in1.val;
        }
        if (cur_in1 & BIT(P2_TH_PIN - 32)) {
            GPIO.out = map1[2];
        }
        else {
            id = ((cur_in0 & BIT(P2_TR_PIN)) >> (P2_TR_PIN - 1)) | ((cur_in1 & BIT(P2_TL_PIN -32)) >> (P2_TL_PIN - 32));
            if (cur_in1 & BIT(P1_TH_PIN - 32)) {
                GPIO.out = *(uint32_t *)&wired_adapter.data[id].output[0];
            }
            else {
                GPIO.out = *(uint32_t *)&wired_adapter.data[id].output[4];
            }
        }
    }
}

void sega_io_init(uint32_t package) {
    gpio_config_t io_conf = {0};
    uint8_t port_cnt = 0;

    dev_type[0] = DEV_NONE;
    dev_type[1] = DEV_NONE;

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
                            mt_dev_type[i][j] = DEV_SEGA_MOUSE;
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
                        dev_type[i] = DEV_SEGA_MOUSE;
                        break;
                }
            }
        }
    }
    else {
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pin_bit_mask = 1ULL << TP_CTRL_PIN;
        gpio_config_iram(&io_conf);
        io_conf.pin_bit_mask = 1ULL << EA_CTRL_PIN;
        gpio_config_iram(&io_conf);
        gpio_set_level_iram(TP_CTRL_PIN, 0);
        gpio_set_level_iram(EA_CTRL_PIN, 0);

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
                gpio_set_level_iram(EA_CTRL_PIN, 1);
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
                            mt_dev_type[i][j] = DEV_SEGA_MOUSE;
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
                        dev_type[i] = DEV_SEGA_MOUSE;
                        break;
                }
            }
        }
    }

    /* TH */
    for (uint32_t i = 0; i < ARRAY_SIZE(gpio_pin); i++) {
        if (dev_type[i] == DEV_SATURN_ANALOG || dev_type[i] == DEV_SATURN_DIGITAL_TWH
            || dev_type[i] == DEV_SATURN_MULTITAP || dev_type[i] == DEV_SATURN_KB) {
            io_conf.intr_type = GPIO_INTR_NEGEDGE;
        }
        else {
            io_conf.intr_type = GPIO_INTR_DISABLE;
        }
        io_conf.pin_bit_mask = 1ULL << gpio_pin[i][SIO_TH];
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config_iram(&io_conf);
    }

    /* TR */
    for (uint32_t i = 0; i < ARRAY_SIZE(gpio_pin); i++) {
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.pin_bit_mask = 1ULL << gpio_pin[i][SIO_TR];
        if (dev_type[i] == DEV_GENESIS_3BTNS || dev_type[i] == DEV_GENESIS_6BTNS || (i == 0 && dev_type[0] == DEV_EA_MULTITAP)) {
            io_conf.mode = GPIO_MODE_OUTPUT;
        }
        else {
            io_conf.mode = GPIO_MODE_INPUT;
        }
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config_iram(&io_conf);
        if (dev_type[i] == DEV_GENESIS_3BTNS || dev_type[i] == DEV_GENESIS_6BTNS) {
            set_sio(i, SIO_TR, 1);
        }
    }

    /* TL, R, L, D, U */
    for (uint32_t i = 0; i < ARRAY_SIZE(gpio_pin); i++) {
        for (uint32_t j = SIO_TL; j <= SIO_U; j++) {
            if (j == SIO_TL && i == 1 && dev_type[1] == DEV_EA_MULTITAP) {
                io_conf.mode = GPIO_MODE_INPUT;
            }
            else {
                io_conf.mode = GPIO_MODE_OUTPUT;
            }
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.pin_bit_mask = 1ULL << gpio_pin[i][j];
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            gpio_config_iram(&io_conf);
            set_sio(i, j, 1);
        }
    }

    /* Init half ID0 */
    for (uint32_t i = 0; i < ARRAY_SIZE(gpio_pin); i++) {
        switch (dev_type[i]) {
            case DEV_GENESIS_3BTNS:
            case DEV_GENESIS_6BTNS:
            case DEV_GENESIS_MULTITAP:
            case DEV_EA_MULTITAP:
                break;
            case DEV_SEGA_MOUSE:
                tx_nibble(i, ID0_MOUSE >> 4);
                if (i == 0) {
                    id0_hi[i] = P1_MOUSE_ID0_HI;
                    id0_lo[i] = P1_MOUSE_ID0_LO;
                }
                else {
                    id0_hi[i] = P2_MOUSE_ID0_HI;
                    id0_lo[i] = P2_MOUSE_ID0_LO;
                }
                break;
            case DEV_SATURN_DIGITAL:
                tx_nibble(i, ID0_SATURN_PAD >> 4);
                if (i == 0) {
                    id0_hi[i] = P1_SAT_PARA_ID0_HI;
                    id0_lo[i] = P1_SAT_PARA_ID0_LO;
                }
                else {
                    id0_hi[i] = P2_SAT_PARA_ID0_HI;
                    id0_lo[i] = P2_SAT_PARA_ID0_LO;
                }
                break;
            case DEV_SATURN_DIGITAL_TWH:
            case DEV_SATURN_ANALOG:
            case DEV_SATURN_MULTITAP:
            case DEV_SATURN_KB:
                tx_nibble(i, ID0_SATURN_THREEWIRE_HANDSHAKE >> 4);
                if (i == 0) {
                    id0_hi[i] = P1_SAT_TWH_ID0_LO_HI;
                    id0_lo[i] = P1_SAT_TWH_ID0_LO_HI;
                }
                else {
                    id0_hi[i] = P2_SAT_TWH_ID0_LO_HI;
                    id0_lo[i] = P2_SAT_TWH_ID0_LO_HI;
                }
                break;
            default:
                ets_printf("Unsupported dev type: %d\n", dev_type[i]);
        }
    }

    if (wired_adapter.system_id == GENESIS) {
        if (dev_type[0] == DEV_EA_MULTITAP) {
            ea_genesis_task();
        }
        else {
            sega_genesis_task();
        }
    }
    else {
        sega_saturn_task();
    }
}
