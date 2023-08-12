/*
 * Copyright (c) 2021-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stddef.h>
#include "npiso_io.h"
#include "cdi_uart.h"
#include "sega_io.h"
#include "pce_io.h"
#include "nsi.h"
#include "maple.h"
#include "jvs_uart.h"
#include "pcfx_spi.h"
#include "ps_spi.h"
#include "real_spi.h"
#include "jag_io.h"
#include "wii_i2c.h"
#include "adapter/adapter.h"
#include "wired_bare.h"

#define SPI_LL_RST_MASK (SPI_OUT_RST | SPI_IN_RST | SPI_AHBM_RST | SPI_AHBM_FIFO_RST)
#define SPI_LL_UNUSED_INT_MASK  (SPI_INT_EN | SPI_SLV_WR_STA_DONE | SPI_SLV_RD_STA_DONE | SPI_SLV_WR_BUF_DONE | SPI_SLV_RD_BUF_DONE)

typedef void (*wired_init_t)(uint32_t package);
typedef void (*wired_port_cfg_t)(uint16_t mask);

static const char *sys_name[WIRED_MAX] = {
    "AUTO",
    "PARALLEL_1P_PP",
    "PARALLEL_2P_PP",
    "NES",
    "PCE",
    "MD-GENESIS",
    "SNES",
    "CD-i",
    "CD32",
    "3DO",
    "JAGUAR",
    "PSX",
    "SATURN",
    "PC-FX",
    "JVS",
    "N64",
    "DC",
    "PS2",
    "GC",
    "Wii-EXT",
    "VB",
    "PARALLEL_1P_OD",
    "PARALLEL_2P_OD",
    "SEA Board",
};

static const wired_init_t wired_init[WIRED_MAX] = {
    NULL, /* WIRED_AUTO */
    NULL, /* PARALLEL_1P */
    NULL, /* PARALLEL_2P */
    npiso_init, /* NES */
    pce_io_init, /* PCE */
    sega_io_init, /* GENESIS */
    npiso_init, /* SNES */
    cdi_uart_init, /* CDI */
    NULL, /* CD32 */
    real_spi_init, /* REAL_3DO */
    jag_io_init, /* JAGUAR */
    ps_spi_init, /* PSX */
    sega_io_init, /* SATURN */
    pcfx_spi_init, /* PCFX */
    jvs_init, /* JVS */
    nsi_init, /* N64 */
    maple_init, /* DC */
    ps_spi_init, /* PS2 */
    nsi_init, /* GC */
    wii_i2c_init, /* WII_EXT */
    npiso_init, /* VB */
    NULL, /* PARALLEL_1P_OD */
    NULL, /* PARALLEL_2P_OD */
    NULL, /* SEA_BOARD */
};

static const wired_port_cfg_t wired_port_cfg[WIRED_MAX] = {
    NULL, /* WIRED_AUTO */
    NULL, /* PARALLEL_1P */
    NULL, /* PARALLEL_2P */
    NULL, /* NES */
    NULL, /* PCE */
    NULL, /* GENESIS */
    NULL, /* SNES */
    NULL, /* CDI */
    NULL, /* CD32 */
    NULL, /* REAL_3DO */
    NULL, /* JAGUAR */
    ps_spi_port_cfg, /* PSX */
    NULL, /* SATURN */
    NULL, /* PCFX */
    NULL, /* JVS */
    nsi_port_cfg, /* N64 */
    maple_port_cfg, /* DC */
    ps_spi_port_cfg, /* PS2 */
    nsi_port_cfg, /* GC */
    wii_i2c_port_cfg, /* WII_EXT */
    NULL, /* VB */
    NULL, /* PARALLEL_1P_OD */
    NULL, /* PARALLEL_2P_OD */
    NULL, /* SEA_BOARD */
};

void wired_bare_init(uint32_t package) {
    if (wired_init[wired_adapter.system_id]) {
        wired_init[wired_adapter.system_id](package);
    }
}

void wired_bare_port_cfg(uint16_t mask) {
    if (wired_port_cfg[wired_adapter.system_id]) {
        wired_port_cfg[wired_adapter.system_id](mask);
    }
}

const char *wired_get_sys_name(void) {
    return sys_name[wired_adapter.system_id];
}

void spi_init(struct spi_cfg *cfg) {
    cfg->hw->clock.val = 0;
    cfg->hw->user.val = 0;
    cfg->hw->ctrl.val = 0;
    cfg->hw->slave.wr_rd_buf_en = 1; //no sure if needed
    cfg->hw->user.doutdin = 1; //we only support full duplex
    cfg->hw->user.sio = 0;
    cfg->hw->slave.slave_mode = 1;
    cfg->hw->dma_conf.val |= SPI_LL_RST_MASK;
    cfg->hw->dma_out_link.start = 0;
    cfg->hw->dma_in_link.start = 0;
    cfg->hw->dma_conf.val &= ~SPI_LL_RST_MASK;
    cfg->hw->slave.sync_reset = 1;
    cfg->hw->slave.sync_reset = 0;

    //use all 64 bytes of the buffer
    cfg->hw->user.usr_miso_highpart = 0;
    cfg->hw->user.usr_mosi_highpart = 0;

    //Disable unneeded ints
    cfg->hw->slave.val &= ~SPI_LL_UNUSED_INT_MASK;

    cfg->hw->ctrl.wr_bit_order = cfg->write_bit_order;
    cfg->hw->ctrl.rd_bit_order = cfg->read_bit_order;

    cfg->hw->pin.ck_idle_edge = cfg->clk_idle_edge;
    cfg->hw->user.ck_i_edge = cfg->clk_i_edge;
    cfg->hw->ctrl2.miso_delay_mode = cfg->miso_delay_mode;
    cfg->hw->ctrl2.miso_delay_num = cfg->miso_delay_num;
    cfg->hw->ctrl2.mosi_delay_mode = cfg->mosi_delay_mode;
    cfg->hw->ctrl2.mosi_delay_num = cfg->mosi_delay_num;

    cfg->hw->slave.sync_reset = 1;
    cfg->hw->slave.sync_reset = 0;

    cfg->hw->slv_wrbuf_dlen.bit_len = cfg->write_bit_len;
    cfg->hw->slv_rdbuf_dlen.bit_len = cfg->read_bit_len;

    cfg->hw->user.usr_miso = 1;
    cfg->hw->user.usr_mosi = 1;

    cfg->hw->data_buf[0] = 0xFF;

    cfg->hw->slave.trans_inten = cfg->inten;
    cfg->hw->slave.trans_done = 0;
    cfg->hw->cmd.usr = 1;
}
