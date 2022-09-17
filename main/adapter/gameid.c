/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "gameid.h"

#define GAME_ID_BUF_LEN 23

static char gameid[GAME_ID_BUF_LEN] = {0};
static char tmp_gameid[GAME_ID_BUF_LEN] = {0};

static void set_pfx_gameid(uint8_t *data, uint32_t len) {
    if (len * 2 >= sizeof(gameid)) {
        len = (sizeof(gameid) - 1) / 2;
    }

    for (uint32_t i = 0; i < len; i++) {
        itoa(data[i], &tmp_gameid[i * 2], 16);
    }
}

static void set_ps1_gameid(uint8_t *data, uint32_t len) {
    if (len >= sizeof(gameid)) {
        len = sizeof(gameid) - 1;
    }
    memcpy(tmp_gameid, data, len);
}

int32_t gid_update(struct raw_fb *fb_data) {
    memset(tmp_gameid, 0, sizeof(gameid));

    switch (wired_adapter.system_id) {
        case N64:
        case GC:
            set_pfx_gameid(fb_data->data, fb_data->header.data_len);
            break;
        case PSX:
        case PS2:
            set_ps1_gameid(fb_data->data, fb_data->header.data_len);
            break;
    }

    if (memcmp(gameid, tmp_gameid, sizeof(gameid)) != 0) {
        memcpy(gameid, tmp_gameid, sizeof(gameid));
        printf("# %s: %s\n", __FUNCTION__, gameid);
        return 1;
    }
    return 0;
}

char *gid_get(void) {
    return gameid;
}