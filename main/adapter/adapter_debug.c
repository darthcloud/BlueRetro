/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <driver/uart.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/adapter_debug.h"
#include "adapter/hid_parser.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "bluetooth/hci.h"
#include "bluetooth/hidp/hidp.h"

#ifdef CONFIG_BLUERETRO_PKT_INJECTION
enum {
    DBG_CMD_CONN = 1,
    DBG_CMD_DISCONN,
    DBG_CMD_NAME,
    DBG_CMD_HID_DESC,
    DBG_CMD_HID_REPORT,
    DBG_CMD_BRIDGE,
    DBG_CMD_GLOBAL_CFG,
    DBG_CMD_OUT_CFG,
    DBG_CMD_IN_CFG,
};

struct adapter_debug_pkt {
    uint8_t cmd;
    uint8_t handle;
    uint16_t data_len;
    uint8_t data[0];
} __packed;

static uint8_t uart_buffer[2048];
#endif

#ifdef CONFIG_BLUERETRO_ADAPTER_BTNS_DBG
static void adapter_debug_btns(int32_t value) {
        uint32_t b = value;
        printf(" %sDL%s %sDR%s %sDD%s %sDU%s %sBL%s %sBR%s %sBD%s %sBU%s %sMM%s %sMS%s %sMT%s %sMQ%s %sLM%s %sLS%s %sLT%s %sLJ%s %sRM%s %sRS%s %sRT%s %sRJ%s",
            (b & BIT(PAD_LD_LEFT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LD_RIGHT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LD_DOWN)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LD_UP)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RB_LEFT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RB_RIGHT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RB_DOWN)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RB_UP)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_MM)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_MS)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_MT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_MQ)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LM)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LS)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LJ)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RM)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RS)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RJ)) ? GREEN : RESET, RESET);
}
#endif

#ifdef CONFIG_BLUERETRO_PKT_INJECTION
static void adapter_debug_task(void *arg) {
    struct adapter_debug_pkt *pkt = (struct adapter_debug_pkt *)uart_buffer;
    int32_t rx_len;
    struct bt_dev *devices[BT_MAX_DEV] = {0};
    struct bt_dev *device = NULL;
    struct bt_data * bt_data = NULL;

    while (1) {
        rx_len = uart_read_bytes(UART_NUM_0, uart_buffer, 4, portMAX_DELAY);
        if (rx_len) {
            rx_len = uart_read_bytes(UART_NUM_0, pkt->data, pkt->data_len, 20 / portTICK_PERIOD_MS);

            if (rx_len != pkt->data_len) {
                printf("# %s data RX timeout\n", __FUNCTION__);
                continue;
            }

            if (pkt->handle >= BT_MAX_DEV) {
                printf("# %s invalid handle\n", __FUNCTION__);
                continue;
            }

            switch(pkt->cmd) {
                case DBG_CMD_CONN:
                    /* Device connection */
                    if (devices[pkt->handle] == NULL) {
                        bt_host_get_new_dev(&devices[pkt->handle]);
                        device = devices[pkt->handle];
                        if (device) {
                            bt_host_reset_dev(device);
                            device->ids.type = BT_HID_GENERIC;
                            atomic_set_bit(&device->flags, BT_DEV_DEVICE_FOUND);
                            printf("# DBG handle: %d dev: %ld type: %ld\n", pkt->handle, device->ids.id, device->ids.type);
                        }
                    }
                    break;
                case DBG_CMD_DISCONN:
                    /* Device disconnection */
                    device = devices[pkt->handle];
                    if (device) {
                        printf("# DBG DISCONN from handle: %d dev: %ld\n", pkt->handle, device->ids.id);
                        bt_host_reset_dev(device);
                        devices[pkt->handle] = NULL;
                    }
                    break;
                case DBG_CMD_NAME:
                    /* Device name */
                    device = devices[pkt->handle];
                    if (device) {
                        bt_hid_set_type_flags_from_name(device, (char *)pkt->data);
                        printf("# dev: %ld type: %ld:%ld %s\n", device->ids.id, device->ids.type, device->ids.subtype, pkt->data);
                    }
                    break;
                case DBG_CMD_HID_DESC:
                    /* HID dewsciptor */
                    device = devices[pkt->handle];
                    if (device) {
                        printf("# %s HID descriptor\n", __FUNCTION__);
                        bt_data = &bt_adapter.data[device->ids.id];
                        hid_parser(bt_data, pkt->data, rx_len - 2);
                    }
                    break;
                case DBG_CMD_HID_REPORT:
                    /* HID reports */
                    device = devices[pkt->handle];
                    if (device) {
                        bt_hid_hdlr(device, (struct bt_hci_pkt *)pkt->data, pkt->data_len);
                    }
                    break;
                case DBG_CMD_BRIDGE:
                    /* Host bridge */
                    device = devices[pkt->handle];
                    if (device) {
                        bt_host_bridge(device, pkt->data[0], &pkt->data[1], pkt->data_len - 1);
                    }
                    break;
                case DBG_CMD_GLOBAL_CFG:
                    /* Global config */
                    memcpy(&config.global_cfg, pkt->data, pkt->data_len);
                    break;
                case DBG_CMD_OUT_CFG:
                    /* Output config */
                    memcpy(&config.out_cfg[pkt->data[0]], &pkt->data[1], pkt->data_len - 1);
                    break;
                case DBG_CMD_IN_CFG:
                    /* Input config */
                    memcpy(&config.in_cfg[pkt->data[0]], &pkt->data[1], pkt->data_len - 1);
                    break;
                default:
                    printf("# %s invalid cmd: 0x%02X\n", __FUNCTION__, pkt->cmd);
                    data_dump(uart_buffer, rx_len);
                    break;
            }
        }
    }
}
#endif

void adapter_debug_print(struct generic_ctrl *ctrl_input) {
        printf("LX: %s%08lX%s, LY: %s%08lX%s, RX: %s%08lX%s, RY: %s%08lX%s, LT: %s%08lX%s, RT: %s%08lX%s, BTNS: %s%08lX%s, BTNS: %s%08lX%s, BTNS: %s%08lX%s, BTNS: %s%08lX%s",
            BOLD, ctrl_input->axes[0].value, RESET, BOLD, ctrl_input->axes[1].value, RESET, BOLD, ctrl_input->axes[2].value, RESET, BOLD, ctrl_input->axes[3].value, RESET,
            BOLD, ctrl_input->axes[4].value, RESET, BOLD, ctrl_input->axes[5].value, RESET, BOLD, ctrl_input->btns[0].value, RESET, BOLD, ctrl_input->btns[1].value, RESET,
            BOLD, ctrl_input->btns[2].value, RESET, BOLD, ctrl_input->btns[3].value, RESET);
#ifdef CONFIG_BLUERETRO_ADAPTER_BTNS_DBG
        adapter_debug_btns(ctrl_input->btns[0].value);
#endif
        printf("\n");
}

void adapter_debug_injector_init(void) {
#ifdef CONFIG_BLUERETRO_PKT_INJECTION
    uart_driver_install(UART_NUM_0, sizeof(uart_buffer), 0, 0, NULL, 0);
    xTaskCreatePinnedToCore(adapter_debug_task, "adapter_debug_task", 4096, NULL, 5, NULL, 0);
#endif
}

