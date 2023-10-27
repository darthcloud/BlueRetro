/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "jag_io.h"
#include "sdkconfig.h"
#if defined (CONFIG_BLUERETRO_SYSTEM_JAGUAR)
#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "adapter/wired/jag.h"
#include "system/core0_stall.h"
#include "system/delay.h"
#include "system/gpio.h"
#include "system/intr.h"

#define P1_J0_PIN 32
#define P1_J1_PIN 33
#define P1_J2_PIN 35
#define P1_J3_PIN 36
#define P1_J8_PIN 18
#define P1_J9_PIN 19
#define P1_J10_PIN 21
#define P1_J11_PIN 22
#define P1_B0_PIN 25
#define P1_B1_PIN 23

#define P1_J0_MASK (1 << (P1_J0_PIN - 32))
#define P1_J1_MASK (1 << (P1_J1_PIN - 32))
#define P1_J2_MASK (1 << (P1_J2_PIN - 32))
#define P1_J3_MASK (1 << (P1_J3_PIN - 32))
#define P1_J8_MASK (1 << P1_J8_PIN)
#define P1_J9_MASK (1 << P1_J9_PIN)
#define P1_J10_MASK (1 << P1_J10_PIN)
#define P1_J11_MASK (1 << P1_J11_PIN)
#define P1_B0_MASK (1 << P1_B0_PIN)
#define P1_B1_MASK (1 << P1_B1_PIN)

#define POLL_TIMEOUT 288


static const uint32_t all_set = 0xFFFDFFFD;
static const uint32_t all_clr = 0x00000000;
static const uint32_t *map_std_tt[][16] = {
    {   /* BANK0 */
        &wired_adapter.data[1].output32[0], /* S1-0 */
        &wired_adapter.data[1].output32[1], /* S1-1 */
        &wired_adapter.data[1].output32[2], /* S1-2 */
        &wired_adapter.data[1].output32[3], /* S1-3 */
        &wired_adapter.data[2].output32[0], /* S2-0 */
        &wired_adapter.data[2].output32[1], /* S2-1 */
        &wired_adapter.data[2].output32[2], /* S2-2 */
        &wired_adapter.data[0].output32[3], /* S0-3 */
        &wired_adapter.data[2].output32[3], /* S2-3 */
        &wired_adapter.data[3].output32[0], /* S3-0 */
        &wired_adapter.data[3].output32[1], /* S3-1 */
        &wired_adapter.data[0].output32[2], /* S0-2 */
        &wired_adapter.data[3].output32[2], /* S3-2 */
        &wired_adapter.data[0].output32[1], /* S0-1 */
        &wired_adapter.data[0].output32[0], /* S0-0 */
        &wired_adapter.data[3].output32[3], /* S3-3 */
    },
    {   /* BANK 1 */
        &wired_adapter.data[1].output32[0], /* S1-0 */
        &wired_adapter.data[1].output32[1], /* S1-1 */
        &wired_adapter.data[1].output32[2], /* S1-2 */
        &wired_adapter.data[1].output32[3], /* S1-3 */
        &wired_adapter.data[2].output32[0], /* S2-0 */
        &wired_adapter.data[2].output32[1], /* S2-1 */
        &wired_adapter.data[2].output32[2], /* S2-2 */
        &wired_adapter.data[0].output32[3], /* S0-3 */
        &wired_adapter.data[2].output32[3], /* S2-3 */
        &wired_adapter.data[3].output32[0], /* S3-0 */
        &wired_adapter.data[3].output32[1], /* S3-1 */
        &wired_adapter.data[0].output32[2], /* S0-2 */
        &wired_adapter.data[3].output32[2], /* S3-2 */
        &wired_adapter.data[0].output32[1], /* S0-1 */
        &wired_adapter.data[0].output32[0], /* S0-0 */
        &wired_adapter.data[3].output32[3], /* S3-3 */
    },
    {   /* BANK 2 */
        &wired_adapter.data[1].output32[0], /* S1-0 */
        &wired_adapter.data[1].output32[1], /* S1-1 */
        &wired_adapter.data[1].output32[2], /* S1-2 */
        &wired_adapter.data[1].output32[3], /* S1-3 */
        &wired_adapter.data[2].output32[0], /* S2-0 */
        &wired_adapter.data[2].output32[1], /* S2-1 */
        &wired_adapter.data[2].output32[2], /* S2-2 */
        &wired_adapter.data[0].output32[3], /* S0-3 */
        &wired_adapter.data[2].output32[3], /* S2-3 */
        &wired_adapter.data[3].output32[0], /* S3-0 */
        &wired_adapter.data[3].output32[1], /* S3-1 */
        &wired_adapter.data[0].output32[2], /* S0-2 */
        &wired_adapter.data[3].output32[2], /* S3-2 */
        &wired_adapter.data[0].output32[1], /* S0-1 */
        &wired_adapter.data[0].output32[0], /* S0-0 */
        &wired_adapter.data[3].output32[3], /* S3-3 */
    },
};

static const uint32_t *map_6d[][16] = {
    {   /* BANK0 */
        &wired_adapter.data[0].output32[0 + 4], /* S1-0 */
        &wired_adapter.data[0].output32[1 + 4], /* S1-1 */
        &wired_adapter.data[0].output32[2 + 4], /* S1-2 */
        &wired_adapter.data[0].output32[3 + 4], /* S1-3 */
        &all_set, /* S2-0 */
        &all_set, /* S2-1 */
        &all_set, /* S2-2 */
        &wired_adapter.data[0].output32[3], /* S0-3 */
        &all_set, /* S2-3 */
        &all_set, /* S3-0 */
        &all_set, /* S3-1 */
        &wired_adapter.data[0].output32[2], /* S0-2 */
        &all_set, /* S3-2 */
        &wired_adapter.data[0].output32[1], /* S0-1 */
        &wired_adapter.data[0].output32[0], /* S0-0 */
        &all_set, /* S3-3 */
    },
    {   /* BANK 1 */
        &wired_adapter.data[0].output32[0 + 8], /* S1-0 */
        &wired_adapter.data[0].output32[1 + 8], /* S1-1 */
        &wired_adapter.data[0].output32[2 + 8], /* S1-2 */
        &wired_adapter.data[0].output32[3 + 8], /* S1-3 */
        &all_set, /* S2-0 */
        &all_set, /* S2-1 */
        &all_set, /* S2-2 */
        &wired_adapter.data[0].output32[3], /* S0-3 */
        &all_set, /* S2-3 */
        &all_set, /* S3-0 */
        &all_set, /* S3-1 */
        &wired_adapter.data[0].output32[2], /* S0-2 */
        &all_set, /* S3-2 */
        &wired_adapter.data[0].output32[1], /* S0-1 */
        &wired_adapter.data[0].output32[0], /* S0-0 */
        &all_set, /* S3-3 */
    },
    {   /* BANK 2 */
        &wired_adapter.data[0].output32[0 + 12], /* S1-0 */
        &wired_adapter.data[0].output32[1 + 12], /* S1-1 */
        &wired_adapter.data[0].output32[2 + 12], /* S1-2 */
        &wired_adapter.data[0].output32[3 + 12], /* S1-3 */
        &all_set, /* S2-0 */
        &all_set, /* S2-1 */
        &all_set, /* S2-2 */
        &wired_adapter.data[0].output32[3], /* S0-3 */
        &all_set, /* S2-3 */
        &all_set, /* S3-0 */
        &all_set, /* S3-1 */
        &wired_adapter.data[0].output32[2], /* S0-2 */
        &all_set, /* S3-2 */
        &wired_adapter.data[0].output32[1], /* S0-1 */
        &wired_adapter.data[0].output32[0], /* S0-0 */
        &all_set, /* S3-3 */
    },
};
static const uint32_t *map_6d_tt[][16] = {
    {   /* BANK0 */
        &wired_adapter.data[1].output32[0 + 4], /* S1-0 */
        &wired_adapter.data[1].output32[1 + 4], /* S1-1 */
        &wired_adapter.data[1].output32[2 + 4], /* S1-2 */
        &wired_adapter.data[1].output32[3 + 4], /* S1-3 */
        &wired_adapter.data[2].output32[0 + 4], /* S2-0 */
        &wired_adapter.data[2].output32[1 + 4], /* S2-1 */
        &wired_adapter.data[2].output32[2 + 4], /* S2-2 */
        &wired_adapter.data[0].output32[3 + 4], /* S0-3 */
        &wired_adapter.data[2].output32[3 + 4], /* S2-3 */
        &wired_adapter.data[3].output32[0 + 4], /* S3-0 */
        &wired_adapter.data[3].output32[1 + 4], /* S3-1 */
        &wired_adapter.data[0].output32[2 + 4], /* S0-2 */
        &wired_adapter.data[3].output32[2 + 4], /* S3-2 */
        &wired_adapter.data[0].output32[1 + 4], /* S0-1 */
        &wired_adapter.data[0].output32[0 + 4], /* S0-0 */
        &wired_adapter.data[3].output32[3 + 4], /* S3-3 */
    },
    {   /* BANK 1 */
        &wired_adapter.data[1].output32[0 + 8], /* S1-0 */
        &wired_adapter.data[1].output32[1 + 8], /* S1-1 */
        &wired_adapter.data[1].output32[2 + 8], /* S1-2 */
        &wired_adapter.data[1].output32[3 + 8], /* S1-3 */
        &wired_adapter.data[2].output32[0 + 8], /* S2-0 */
        &wired_adapter.data[2].output32[1 + 8], /* S2-1 */
        &wired_adapter.data[2].output32[2 + 8], /* S2-2 */
        &wired_adapter.data[0].output32[3 + 8], /* S0-3 */
        &wired_adapter.data[2].output32[3 + 8], /* S2-3 */
        &wired_adapter.data[3].output32[0 + 8], /* S3-0 */
        &wired_adapter.data[3].output32[1 + 8], /* S3-1 */
        &wired_adapter.data[0].output32[2 + 8], /* S0-2 */
        &wired_adapter.data[3].output32[2 + 8], /* S3-2 */
        &wired_adapter.data[0].output32[1 + 8], /* S0-1 */
        &wired_adapter.data[0].output32[0 + 8], /* S0-0 */
        &wired_adapter.data[3].output32[3 + 8], /* S3-3 */
    },
    {   /* BANK 2 */
        &wired_adapter.data[1].output32[0 + 12], /* S1-0 */
        &wired_adapter.data[1].output32[1 + 12], /* S1-1 */
        &wired_adapter.data[1].output32[2 + 12], /* S1-2 */
        &wired_adapter.data[1].output32[3 + 12], /* S1-3 */
        &wired_adapter.data[2].output32[0 + 12], /* S2-0 */
        &wired_adapter.data[2].output32[1 + 12], /* S2-1 */
        &wired_adapter.data[2].output32[2 + 12], /* S2-2 */
        &wired_adapter.data[0].output32[3 + 12], /* S0-3 */
        &wired_adapter.data[2].output32[3 + 12], /* S2-3 */
        &wired_adapter.data[3].output32[0 + 12], /* S3-0 */
        &wired_adapter.data[3].output32[1 + 12], /* S3-1 */
        &wired_adapter.data[0].output32[2 + 12], /* S0-2 */
        &wired_adapter.data[3].output32[2 + 12], /* S3-2 */
        &wired_adapter.data[0].output32[1 + 12], /* S0-1 */
        &wired_adapter.data[0].output32[0 + 12], /* S0-0 */
        &wired_adapter.data[3].output32[3 + 12], /* S3-3 */
    },
};
static const uint32_t *map_std_tt_mask[][16] = {
    {   /* BANK0 */
        &wired_adapter.data[1].output_mask32[0], /* S1-0 */
        &wired_adapter.data[1].output_mask32[1], /* S1-1 */
        &wired_adapter.data[1].output_mask32[2], /* S1-2 */
        &wired_adapter.data[1].output_mask32[3], /* S1-3 */
        &wired_adapter.data[2].output_mask32[0], /* S2-0 */
        &wired_adapter.data[2].output_mask32[1], /* S2-1 */
        &wired_adapter.data[2].output_mask32[2], /* S2-2 */
        &wired_adapter.data[0].output_mask32[3], /* S0-3 */
        &wired_adapter.data[2].output_mask32[3], /* S2-3 */
        &wired_adapter.data[3].output_mask32[0], /* S3-0 */
        &wired_adapter.data[3].output_mask32[1], /* S3-1 */
        &wired_adapter.data[0].output_mask32[2], /* S0-2 */
        &wired_adapter.data[3].output_mask32[2], /* S3-2 */
        &wired_adapter.data[0].output_mask32[1], /* S0-1 */
        &wired_adapter.data[0].output_mask32[0], /* S0-0 */
        &wired_adapter.data[3].output_mask32[3], /* S3-3 */
    },
    {   /* BANK 1 */
        &wired_adapter.data[1].output_mask32[0], /* S1-0 */
        &wired_adapter.data[1].output_mask32[1], /* S1-1 */
        &wired_adapter.data[1].output_mask32[2], /* S1-2 */
        &wired_adapter.data[1].output_mask32[3], /* S1-3 */
        &wired_adapter.data[2].output_mask32[0], /* S2-0 */
        &wired_adapter.data[2].output_mask32[1], /* S2-1 */
        &wired_adapter.data[2].output_mask32[2], /* S2-2 */
        &wired_adapter.data[0].output_mask32[3], /* S0-3 */
        &wired_adapter.data[2].output_mask32[3], /* S2-3 */
        &wired_adapter.data[3].output_mask32[0], /* S3-0 */
        &wired_adapter.data[3].output_mask32[1], /* S3-1 */
        &wired_adapter.data[0].output_mask32[2], /* S0-2 */
        &wired_adapter.data[3].output_mask32[2], /* S3-2 */
        &wired_adapter.data[0].output_mask32[1], /* S0-1 */
        &wired_adapter.data[0].output_mask32[0], /* S0-0 */
        &wired_adapter.data[3].output_mask32[3], /* S3-3 */
    },
    {   /* BANK 2 */
        &wired_adapter.data[1].output_mask32[0], /* S1-0 */
        &wired_adapter.data[1].output_mask32[1], /* S1-1 */
        &wired_adapter.data[1].output_mask32[2], /* S1-2 */
        &wired_adapter.data[1].output_mask32[3], /* S1-3 */
        &wired_adapter.data[2].output_mask32[0], /* S2-0 */
        &wired_adapter.data[2].output_mask32[1], /* S2-1 */
        &wired_adapter.data[2].output_mask32[2], /* S2-2 */
        &wired_adapter.data[0].output_mask32[3], /* S0-3 */
        &wired_adapter.data[2].output_mask32[3], /* S2-3 */
        &wired_adapter.data[3].output_mask32[0], /* S3-0 */
        &wired_adapter.data[3].output_mask32[1], /* S3-1 */
        &wired_adapter.data[0].output_mask32[2], /* S0-2 */
        &wired_adapter.data[3].output_mask32[2], /* S3-2 */
        &wired_adapter.data[0].output_mask32[1], /* S0-1 */
        &wired_adapter.data[0].output_mask32[0], /* S0-0 */
        &wired_adapter.data[3].output_mask32[3], /* S3-3 */
    },
};

static const uint32_t *map_6d_mask[][16] = {
    {   /* BANK0 */
        &wired_adapter.data[0].output_mask32[0 + 4], /* S1-0 */
        &wired_adapter.data[0].output_mask32[1 + 4], /* S1-1 */
        &wired_adapter.data[0].output_mask32[2 + 4], /* S1-2 */
        &wired_adapter.data[0].output_mask32[3 + 4], /* S1-3 */
        &all_clr, /* S2-0 */
        &all_clr, /* S2-1 */
        &all_clr, /* S2-2 */
        &wired_adapter.data[0].output_mask32[3], /* S0-3 */
        &all_clr, /* S2-3 */
        &all_clr, /* S3-0 */
        &all_clr, /* S3-1 */
        &wired_adapter.data[0].output_mask32[2], /* S0-2 */
        &all_clr, /* S3-2 */
        &wired_adapter.data[0].output_mask32[1], /* S0-1 */
        &wired_adapter.data[0].output_mask32[0], /* S0-0 */
        &all_clr, /* S3-3 */
    },
    {   /* BANK 1 */
        &wired_adapter.data[0].output_mask32[0 + 8], /* S1-0 */
        &wired_adapter.data[0].output_mask32[1 + 8], /* S1-1 */
        &wired_adapter.data[0].output_mask32[2 + 8], /* S1-2 */
        &wired_adapter.data[0].output_mask32[3 + 8], /* S1-3 */
        &all_clr, /* S2-0 */
        &all_clr, /* S2-1 */
        &all_clr, /* S2-2 */
        &wired_adapter.data[0].output_mask32[3], /* S0-3 */
        &all_clr, /* S2-3 */
        &all_clr, /* S3-0 */
        &all_clr, /* S3-1 */
        &wired_adapter.data[0].output_mask32[2], /* S0-2 */
        &all_clr, /* S3-2 */
        &wired_adapter.data[0].output_mask32[1], /* S0-1 */
        &wired_adapter.data[0].output_mask32[0], /* S0-0 */
        &all_clr, /* S3-3 */
    },
    {   /* BANK 2 */
        &wired_adapter.data[0].output_mask32[0 + 12], /* S1-0 */
        &wired_adapter.data[0].output_mask32[1 + 12], /* S1-1 */
        &wired_adapter.data[0].output_mask32[2 + 12], /* S1-2 */
        &wired_adapter.data[0].output_mask32[3 + 12], /* S1-3 */
        &all_clr, /* S2-0 */
        &all_clr, /* S2-1 */
        &all_clr, /* S2-2 */
        &wired_adapter.data[0].output_mask32[3], /* S0-3 */
        &all_clr, /* S2-3 */
        &all_clr, /* S3-0 */
        &all_clr, /* S3-1 */
        &wired_adapter.data[0].output_mask32[2], /* S0-2 */
        &all_clr, /* S3-2 */
        &wired_adapter.data[0].output_mask32[1], /* S0-1 */
        &wired_adapter.data[0].output_mask32[0], /* S0-0 */
        &all_clr, /* S3-3 */
    },
};
static const uint32_t *map_6d_tt_mask[][16] = {
    {   /* BANK0 */
        &wired_adapter.data[1].output_mask32[0 + 4], /* S1-0 */
        &wired_adapter.data[1].output_mask32[1 + 4], /* S1-1 */
        &wired_adapter.data[1].output_mask32[2 + 4], /* S1-2 */
        &wired_adapter.data[1].output_mask32[3 + 4], /* S1-3 */
        &wired_adapter.data[2].output_mask32[0 + 4], /* S2-0 */
        &wired_adapter.data[2].output_mask32[1 + 4], /* S2-1 */
        &wired_adapter.data[2].output_mask32[2 + 4], /* S2-2 */
        &wired_adapter.data[0].output_mask32[3 + 4], /* S0-3 */
        &wired_adapter.data[2].output_mask32[3 + 4], /* S2-3 */
        &wired_adapter.data[3].output_mask32[0 + 4], /* S3-0 */
        &wired_adapter.data[3].output_mask32[1 + 4], /* S3-1 */
        &wired_adapter.data[0].output_mask32[2 + 4], /* S0-2 */
        &wired_adapter.data[3].output_mask32[2 + 4], /* S3-2 */
        &wired_adapter.data[0].output_mask32[1 + 4], /* S0-1 */
        &wired_adapter.data[0].output_mask32[0 + 4], /* S0-0 */
        &wired_adapter.data[3].output_mask32[3 + 4], /* S3-3 */
    },
    {   /* BANK 1 */
        &wired_adapter.data[1].output_mask32[0 + 8], /* S1-0 */
        &wired_adapter.data[1].output_mask32[1 + 8], /* S1-1 */
        &wired_adapter.data[1].output_mask32[2 + 8], /* S1-2 */
        &wired_adapter.data[1].output_mask32[3 + 8], /* S1-3 */
        &wired_adapter.data[2].output_mask32[0 + 8], /* S2-0 */
        &wired_adapter.data[2].output_mask32[1 + 8], /* S2-1 */
        &wired_adapter.data[2].output_mask32[2 + 8], /* S2-2 */
        &wired_adapter.data[0].output_mask32[3 + 8], /* S0-3 */
        &wired_adapter.data[2].output_mask32[3 + 8], /* S2-3 */
        &wired_adapter.data[3].output_mask32[0 + 8], /* S3-0 */
        &wired_adapter.data[3].output_mask32[1 + 8], /* S3-1 */
        &wired_adapter.data[0].output_mask32[2 + 8], /* S0-2 */
        &wired_adapter.data[3].output_mask32[2 + 8], /* S3-2 */
        &wired_adapter.data[0].output_mask32[1 + 8], /* S0-1 */
        &wired_adapter.data[0].output_mask32[0 + 8], /* S0-0 */
        &wired_adapter.data[3].output_mask32[3 + 8], /* S3-3 */
    },
    {   /* BANK 2 */
        &wired_adapter.data[1].output_mask32[0 + 12], /* S1-0 */
        &wired_adapter.data[1].output_mask32[1 + 12], /* S1-1 */
        &wired_adapter.data[1].output_mask32[2 + 12], /* S1-2 */
        &wired_adapter.data[1].output_mask32[3 + 12], /* S1-3 */
        &wired_adapter.data[2].output_mask32[0 + 12], /* S2-0 */
        &wired_adapter.data[2].output_mask32[1 + 12], /* S2-1 */
        &wired_adapter.data[2].output_mask32[2 + 12], /* S2-2 */
        &wired_adapter.data[0].output_mask32[3 + 12], /* S0-3 */
        &wired_adapter.data[2].output_mask32[3 + 12], /* S2-3 */
        &wired_adapter.data[3].output_mask32[0 + 12], /* S3-0 */
        &wired_adapter.data[3].output_mask32[1 + 12], /* S3-1 */
        &wired_adapter.data[0].output_mask32[2 + 12], /* S0-2 */
        &wired_adapter.data[3].output_mask32[2 + 12], /* S3-2 */
        &wired_adapter.data[0].output_mask32[1 + 12], /* S0-1 */
        &wired_adapter.data[0].output_mask32[0 + 12], /* S0-0 */
        &wired_adapter.data[3].output_mask32[3 + 12], /* S3-3 */
    },
};
static const uint8_t socket_idx[16] = {
    1, 1, 1, 1, 2, 2, 2, 0, 2, 3, 3, 0, 3, 0, 0, 3
};
static const uint8_t row_idx[16] = {
    0, 1, 2, 3, 0, 1, 2, 3, 3, 0, 1, 2, 2, 1, 0, 3
};

static const uint32_t *(*map)[16];
static const uint32_t *(*map_mask)[16];
static uint8_t bank[4] = {0};

static void jag_ctrl_task(void) {
    uint32_t timeout, cur_in1, prev_in1, idx, change1 = 0, lock = 0;

    timeout = 0;
    cur_in1 = GPIO.in1.val;
    while (1) {
        prev_in1 = cur_in1;
        cur_in1 = GPIO.in1.val;
        while (!(change1 = cur_in1 ^ prev_in1)) {
            prev_in1 = cur_in1;
            cur_in1 = GPIO.in1.val;
            if (lock) {
                if (++timeout > POLL_TIMEOUT) {
                    core0_stall_end();
                    lock = 0;
                    timeout = 0;
                }
            }
        }

        if (change1 & 0x1B) {
            if (!lock) {
                core0_stall_start();
                ++lock;
            }
            idx = (((cur_in1 & 0x18) >> 1) | (cur_in1 & 0x3)) & 0xF;

            GPIO.out = *map[bank[socket_idx[idx]]][idx] | *map_mask[bank[socket_idx[idx]]][idx];
            timeout = 0;

            if (row_idx[idx] == 3) {
                ++wired_adapter.data[socket_idx[idx]].frame_cnt;
                jag_gen_turbo_mask(&wired_adapter.data[socket_idx[idx]]);
                bank[socket_idx[idx]]++;
                if (bank[socket_idx[idx]] > 2) {
                    bank[socket_idx[idx]] = 0;
                }
            }
        }
    }
}
#endif /* defined (CONFIG_BLUERETRO_SYSTEM_JAGUAR */

void jag_io_force_update(void) {
#if defined (CONFIG_BLUERETRO_SYSTEM_JAGUAR)
    uint32_t idx, cur_in1 = GPIO.in1.val;

    idx = (((cur_in1 & 0x18) >> 1) | (cur_in1 & 0x3)) & 0xF;

    GPIO.out = *map[bank[socket_idx[idx]]][idx];
#endif /* defined (CONFIG_BLUERETRO_SYSTEM_JAGUAR */
}

void jag_io_init(uint32_t package) {
#if defined (CONFIG_BLUERETRO_SYSTEM_JAGUAR)
    gpio_config_t io_conf = {0};
    uint8_t inputs[] = {P1_J0_PIN, P1_J1_PIN, P1_J2_PIN, P1_J3_PIN};
    uint8_t outputs[] = {P1_J8_PIN, P1_J9_PIN, P1_J10_PIN, P1_J11_PIN, P1_B0_PIN, P1_B1_PIN};

    /* Inputs */
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    for (uint32_t i = 0; i < ARRAY_SIZE(inputs); i++) {
        io_conf.pin_bit_mask = 1ULL << inputs[i];
        gpio_config_iram(&io_conf);
    }

    /* Outputs */
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    for (uint32_t i = 0; i < ARRAY_SIZE(outputs); i++) {
        io_conf.pin_bit_mask = 1ULL << outputs[i];
        gpio_config_iram(&io_conf);
        gpio_set_level_iram(outputs[i], 1);
    }

    if (config.out_cfg[0].dev_mode == DEV_PAD_ALT) {
        if (config.global_cfg.multitap_cfg == MT_SLOT_1) {
            map = map_6d_tt;
            map_mask = map_6d_tt_mask;
        }
        else {
            map = map_6d;
            map_mask = map_6d_mask;
        }
    }
    else {
        map = map_std_tt;
        map_mask = map_std_tt_mask;
    }
    jag_ctrl_task();
#endif /* defined (CONFIG_BLUERETRO_SYSTEM_JAGUAR */
}
