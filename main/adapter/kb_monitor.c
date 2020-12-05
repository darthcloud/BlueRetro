/*
 * Copyright (c) 2019-2020, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include "../zephyr/types.h"
#include "../util.h"
#include "kb_monitor.h"

struct kb_monitor {
    kbmon_id_to_code_t callback;
    RingbufHandle_t scq_hdl;
    uint32_t keys_state[4];
};

static struct kb_monitor kb_monitors[BT_MAX_DEV] = {0};

void kbmon_init(uint8_t dev_id, kbmon_id_to_code_t callback) {
    if (dev_id < BT_MAX_DEV) {
        kb_monitors[dev_id].callback = callback;
        kb_monitors[dev_id].scq_hdl = xRingbufferCreate(64, RINGBUF_TYPE_NOSPLIT);
        if (kb_monitors[dev_id].scq_hdl == NULL) {
            printf("# Failed to create KBMON:%d ring buffer\n", dev_id);
        }
    }
}

void kbmon_update(uint8_t dev_id, struct generic_ctrl *ctrl_data) {
    uint32_t keys_change[4];

    for (uint32_t i = 0; i < 4; i++) {
        keys_change[i] = ctrl_data->btns[i].value ^ kb_monitors[dev_id].keys_state[i];
        kb_monitors[dev_id].keys_state[i] = ctrl_data->btns[i].value;
    }

    for (uint32_t i = 0; i < KBM_MAX; i++) {
        if (ctrl_data->map_mask[i / 32] & BIT(i & 0x1F)) {
            if (keys_change[i / 32] & BIT(i & 0x1F)) {
                if (ctrl_data->btns[i / 32].value & BIT(i & 0x1F)) {
                    kb_monitors[dev_id].callback(dev_id, KBMON_TYPE_MAKE, i);
                }
                else {
                    kb_monitors[dev_id].callback(dev_id, KBMON_TYPE_BREAK, i);
                }
            }
        }
    }
}

int32_t kbmon_set_code(uint8_t dev_id, uint8_t *code, uint32_t len) {
    UBaseType_t ret = pdFALSE;

    if (kb_monitors[dev_id].scq_hdl) {
        ret = xRingbufferSend(kb_monitors[dev_id].scq_hdl, (void *)code, len, 0);
        if (ret != pdTRUE) {
            printf("# %s KBMON:%d scq full!\n", __FUNCTION__, dev_id);
        }
    }
    return (ret == pdTRUE ? 0 : -1);
}

int32_t kbmon_get_code(uint8_t dev_id, uint8_t *code, uint32_t *len) {
    int32_t ret = -1;
    uint32_t qlen;
    if (kb_monitors[dev_id].scq_hdl) {
        uint8_t *qcode = (uint8_t *)xRingbufferReceive(kb_monitors[dev_id].scq_hdl, &qlen, 0);
        if (qcode) {
            memcpy(code, qcode, qlen);
            vRingbufferReturnItem(kb_monitors[dev_id].scq_hdl, (void *)qcode);
            *len = qlen;
            ret = 0;
        }
    }
    return ret;
}
