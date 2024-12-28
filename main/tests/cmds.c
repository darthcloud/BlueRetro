/*
 * Copyright (c) 2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdarg.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/hid_parser.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "bluetooth/hci.h"
#include "bluetooth/hidp/hidp.h"
#include "cmds.h"

enum {
    TESTS_CMDS_CONN = 1,
    TESTS_CMDS_DISCONN,
    TESTS_CMDS_NAME,
    TESTS_CMDS_HID_DESC,
    TESTS_CMDS_HID_REPORT,
    TESTS_CMDS_BRIDGE,
    TESTS_CMDS_GLOBAL_CFG,
    TESTS_CMDS_OUT_CFG,
    TESTS_CMDS_IN_CFG,
    TESTS_CMDS_SYSTEM_ID,
};

static uint32_t log_offset = 0;
static char log_buffer[4096];

char *tests_cmds_hdlr(struct tests_cmds_pkt *pkt) {
    static struct bt_dev *devices[BT_MAX_DEV] = {0};
    struct bt_dev *device = NULL;
    struct bt_data * bt_data = NULL;

    memset(log_buffer, 0, sizeof(log_buffer));
    log_offset = 0;
    TESTS_CMDS_LOG("{}\n");
    log_offset = 1;

    switch(pkt->cmd) {
        case TESTS_CMDS_CONN:
            /* Device connection */
            if (devices[pkt->handle] == NULL) {
                bt_host_get_new_dev(&devices[pkt->handle]);
                device = devices[pkt->handle];
                if (device) {
                    bt_host_reset_dev(device);
                    device->ids.type = BT_HID_GENERIC;
                    atomic_set_bit(&device->flags, BT_DEV_DEVICE_FOUND);
                    if (pkt->data_len && pkt->data[0]) {
                        atomic_set_bit(&device->flags, BT_DEV_IS_BLE);
                    }
                    printf("# TESTS handle: %d dev: %ld type: %ld\n", pkt->handle, device->ids.id, device->ids.type);
                    TESTS_CMDS_LOG("\"device_conn\": {\"handle\": %d, \"device_id\": %d, \"device_type\": %d},\n",
                        pkt->handle, device->ids.id, device->ids.type);
                }
            }
            break;
        case TESTS_CMDS_DISCONN:
            /* Device disconnection */
            device = devices[pkt->handle];
            if (device) {
                printf("# TESTS DISCONN from handle: %d dev: %ld\n", pkt->handle, device->ids.id);
                TESTS_CMDS_LOG("\"device_disconn\": {\"handle\": %d, \"device_id\": %d},\n",
                    pkt->handle, device->ids.id);
                bt_host_reset_dev(device);
                devices[pkt->handle] = NULL;
            }
            break;
        case TESTS_CMDS_NAME:
            /* Device name */
            device = devices[pkt->handle];
            if (device) {
                bt_hid_set_type_flags_from_name(device, (char *)pkt->data);
                printf("# dev: %ld type: %ld:%ld %s\n", device->ids.id, device->ids.type, device->ids.subtype, pkt->data);
                TESTS_CMDS_LOG("\"device_name\": {\"device_id\": %d, \"device_type\": %d, \"device_subtype\": %d, \"device_name\": \"%s\"},\n",
                    device->ids.id, device->ids.type, device->ids.subtype, pkt->data);
                bt_hid_init(device);
            }
            break;
        case TESTS_CMDS_HID_DESC:
            /* HID dewsciptor */
            device = devices[pkt->handle];
            if (device) {
                printf("# %s HID descriptor\n", __FUNCTION__);
                bt_data = &bt_adapter.data[device->ids.id];
                hid_parser(bt_data, pkt->data, pkt->data_len);
            }
            break;
        case TESTS_CMDS_HID_REPORT:
            /* HID reports */
            device = devices[pkt->handle];
            if (device) {
                bt_hid_hdlr(device, (struct bt_hci_pkt *)pkt->data, pkt->data_len);
            }
            break;
        case TESTS_CMDS_BRIDGE:
            /* Host bridge */
            device = devices[pkt->handle];
            if (device) {
                bt_host_bridge(device, pkt->data[0], &pkt->data[1], pkt->data_len - 1);
            }
            break;
        case TESTS_CMDS_GLOBAL_CFG:
            /* Global config */
            memcpy(&config.global_cfg, pkt->data, pkt->data_len);
            break;
        case TESTS_CMDS_OUT_CFG:
            /* Output config */
            memcpy(&config.out_cfg[pkt->data[0]], &pkt->data[1], pkt->data_len - 1);
            break;
        case TESTS_CMDS_IN_CFG:
            /* Input config */
            memcpy(&config.in_cfg[pkt->data[0]], &pkt->data[1], pkt->data_len - 1);
            break;
        case TESTS_CMDS_SYSTEM_ID:
            /* Set System ID */
            wired_adapter.system_id = pkt->data[0];
            break;
        default:
            printf("# %s invalid cmd: 0x%02X\n", __FUNCTION__, pkt->cmd);
            break;
    }
    log_buffer[log_offset - 2] = '}';
    return log_buffer;
}

void tests_cmds_log(const char * format, ...) {
    va_list args;
    va_start(args, format);
    size_t max_len = sizeof(log_buffer) - log_offset;
    int len = vsnprintf(log_buffer + log_offset, max_len, format, args);
    if (len > 0 && len < max_len) {
        log_offset += len;
    }
    va_end(args);
}
