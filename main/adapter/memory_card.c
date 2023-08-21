/*
 * Copyright (c) 2021-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <soc/soc_memory_layout.h>
#include "xtensa/core-macros.h"
#include "sdkconfig.h"
#include "system/fs.h"
#include "memory_card.h"

#define MC_BUFFER_SIZE (128 * 1024)
#define MC_BUFFER_BLOCK_SIZE (4 * 1024)
#define MC_BUFFER_BLOCK_CNT (MC_BUFFER_SIZE / MC_BUFFER_BLOCK_SIZE)

/* Related to bare metal hack, something goes writing there. */
/* Workaround: Remove region from heap pool until I figure it out */
SOC_RESERVE_MEMORY_REGION(0x3FFE7D98, 0x3FFE7E28, bad_region);

static uint8_t *mc_buffer[MC_BUFFER_BLOCK_CNT] = {0};
static esp_timer_handle_t mc_timer_hdl = NULL;
static int32_t mc_block_state = 0;

static int32_t mc_restore(void);
static int32_t mc_store(void);
static inline void mc_store_cb(void *arg);

static void mc_start_update_timer(uint64_t timeout_us) {
    if (mc_timer_hdl) {
        if (esp_timer_is_active(mc_timer_hdl)) {
            esp_timer_stop(mc_timer_hdl);
        }
        esp_timer_start_once(mc_timer_hdl, timeout_us);
    }
}

static int32_t mc_restore(void) {
    struct stat st;
    int32_t ret = -1;

    if (stat(MEMORY_CARD_FILE, &st) != 0) {
        printf("# %s: No Memory Card on FS. Creating...\n", __FUNCTION__);
        ret = mc_store();
    }
    else {
        FILE *file = fopen(MEMORY_CARD_FILE, "rb");
        if (file == NULL) {
            printf("# %s: failed to open file for reading\n", __FUNCTION__);
        }
        else {
            uint32_t count = 0;
            for (uint32_t i = 0; i < MC_BUFFER_BLOCK_CNT; i++) {
                count += fread((void *)mc_buffer[i], MC_BUFFER_BLOCK_SIZE, 1, file);
            }
            fclose(file);

            if (count == MC_BUFFER_BLOCK_CNT) {
                ret = 0;
                printf("# %s: restore sucessful!\n", __FUNCTION__);
            }
            else {
                printf("# %s: restore failed! cnt: %ld File size:%ld\n", __FUNCTION__, count, st.st_size);
            }
        }
    }
    return ret;
}

static int32_t mc_store(void) {
    int32_t ret = -1;

    FILE *file = fopen(MEMORY_CARD_FILE, "wb");
    if (file == NULL) {
        printf("# %s: failed to open file for writing\n", __FUNCTION__);
    }
    else {
        uint32_t count = 0;
        for (uint32_t i = 0; i < MC_BUFFER_BLOCK_CNT; i++) {
            count += fwrite((void *)mc_buffer[i], MC_BUFFER_BLOCK_SIZE, 1, file);
        }
        fclose(file);

        if (count == MC_BUFFER_BLOCK_CNT) {
            ret = 0;
            mc_block_state = 0;
        }
        printf("# %s: file updated cnt: %ld\n", __FUNCTION__, count);
    }
    return ret;
}

static int32_t mc_store_spread(void) {
    int32_t ret = -1;

    FILE *file = fopen(MEMORY_CARD_FILE, "r+b");
    if (file == NULL) {
        printf("# %s: failed to open file for writing\n", __FUNCTION__);
    }
    else {
        uint32_t block = __builtin_ffs(mc_block_state);

        if (block) {
            uint32_t count = 0;
            block -= 1;

            fseek(file, block * MC_BUFFER_BLOCK_SIZE, SEEK_SET);
            count = fwrite((void *)mc_buffer[block], MC_BUFFER_BLOCK_SIZE, 1, file);
            fclose(file);

            if (count == 1) {
                ret = 0;
                atomic_clear_bit(&mc_block_state, block);
            }

            printf("# %s: block %ld updated cnt: %ld\n", __FUNCTION__, block, count);

            if (mc_block_state) {
                mc_start_update_timer(20000);
            }
        }
    }
    return ret;
}

static inline void mc_store_cb(void *arg) {
    (void)mc_store_spread();
}

int32_t mc_init(void) {
    int32_t ret = -1;
    const esp_timer_create_args_t mc_timer_args = {
        .callback = &mc_store_cb,
        .arg = (void *)NULL,
        .name = "mc_timer"
    };

    for (uint32_t i = 0; i < MC_BUFFER_BLOCK_CNT; i++) {
        mc_buffer[i] = malloc(MC_BUFFER_BLOCK_SIZE);

        if (mc_buffer[i] == NULL) {
            printf("# %s mc_buffer[%ld] alloc fail\n", __FUNCTION__, i);
            heap_caps_dump_all();
            goto exit;
        }
    }

    esp_timer_create(&mc_timer_args, &mc_timer_hdl);

    ret = mc_restore();

    return ret;

exit:
    return -1;
}

void mc_storage_update(void) {
    mc_start_update_timer(1000000);
}

/* Assume r/w size will never cross blocks boundary */
void IRAM_ATTR mc_read(uint32_t addr, uint8_t *data, uint32_t size) {
    memcpy(data, mc_buffer[addr >> 12] + (addr & 0xFFF), size);
}

void IRAM_ATTR mc_write(uint32_t addr, uint8_t *data, uint32_t size) {
    struct raw_fb fb_data = {0};
    uint32_t block = addr >> 12;

    memcpy(mc_buffer[block] + (addr & 0xFFF), data, size);
    atomic_set_bit(&mc_block_state, block);

    fb_data.header.wired_id = 0;
    fb_data.header.type = FB_TYPE_MEM_WRITE;
    fb_data.header.data_len = 0;
    adapter_q_fb(&fb_data);
}

uint8_t IRAM_ATTR *mc_get_ptr(uint32_t addr) {
    return mc_buffer[addr >> 12] + (addr & 0xFFF);
}
