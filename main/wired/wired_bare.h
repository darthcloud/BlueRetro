/*
 * Copyright (c) 2021-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _WIRED_BARE_H_
#define _WIRED_BARE_H_

#include <soc/spi_periph.h>

#if defined(CONFIG_BLUERETRO_SYSTEM_PARALLEL_1P)
#define HARDCODED_SYS PARALLEL_1P
#elif defined(CONFIG_BLUERETRO_SYSTEM_PARALLEL_2P)
#define HARDCODED_SYS PARALLEL_2P
#elif defined(CONFIG_BLUERETRO_SYSTEM_NES)
#define HARDCODED_SYS NES
#elif defined(CONFIG_BLUERETRO_SYSTEM_PCE)
#define HARDCODED_SYS PCE
#elif defined(CONFIG_BLUERETRO_SYSTEM_GENESIS)
#define HARDCODED_SYS GENESIS
#elif defined(CONFIG_BLUERETRO_SYSTEM_SNES)
#define HARDCODED_SYS SNES
#elif defined(CONFIG_BLUERETRO_SYSTEM_CDI)
#define HARDCODED_SYS CDI
#elif defined(CONFIG_BLUERETRO_SYSTEM_CD32)
#define HARDCODED_SYS CD32
#elif defined(CONFIG_BLUERETRO_SYSTEM_3DO)
#define HARDCODED_SYS REAL_3DO
#elif defined(CONFIG_BLUERETRO_SYSTEM_JAGUAR)
#define HARDCODED_SYS JAGUAR
#elif defined(CONFIG_BLUERETRO_SYSTEM_PSX_PS2)
#define HARDCODED_SYS PS2
#elif defined(CONFIG_BLUERETRO_SYSTEM_SATURN)
#define HARDCODED_SYS SATURN
#elif defined(CONFIG_BLUERETRO_SYSTEM_PCFX)
#define HARDCODED_SYS PCFX
#elif defined(CONFIG_BLUERETRO_SYSTEM_JVS)
#define HARDCODED_SYS JVS
#elif defined(CONFIG_BLUERETRO_SYSTEM_N64)
#define HARDCODED_SYS N64
#elif defined(CONFIG_BLUERETRO_SYSTEM_DC)
#define HARDCODED_SYS DC
#elif defined(CONFIG_BLUERETRO_SYSTEM_GC)
#define HARDCODED_SYS GC
#elif defined(CONFIG_BLUERETRO_SYSTEM_WII_EXT)
#define HARDCODED_SYS WII_EXT
#elif defined(CONFIG_BLUERETRO_SYSTEM_VBOY)
#define HARDCODED_SYS VBOY
#elif defined(CONFIG_BLUERETRO_SYSTEM_PARALLEL_1P_OD)
#define HARDCODED_SYS PARALLEL_1P_OD
#elif defined(CONFIG_BLUERETRO_SYSTEM_PARALLEL_2P_OD)
#define HARDCODED_SYS PARALLEL_2P_OD
#elif defined(CONFIG_BLUERETRO_SYSTEM_SEA_BOARD)
#define HARDCODED_SYS SEA_BOARD
#elif defined(CONFIG_BLUERETRO_SYSTEM_SPI)
#define HARDCODED_SYS SPI
#endif

struct spi_cfg {
    spi_dev_t *hw;
    uint32_t write_bit_order;
    uint32_t read_bit_order;
    uint32_t clk_idle_edge;
    uint32_t clk_i_edge;
    uint32_t miso_delay_mode;
    uint32_t miso_delay_num;
    uint32_t mosi_delay_mode;
    uint32_t mosi_delay_num;
    uint32_t write_bit_len;
    uint32_t read_bit_len;
    uint32_t inten;
};

void wired_bare_init(uint32_t package);
void wired_bare_port_cfg(uint16_t mask);
const char *wired_get_sys_name(void);
void spi_init(struct spi_cfg *cfg);

#endif /* _WIRED_BARE_H_ */
