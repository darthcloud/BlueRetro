#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "bt.h"
#include "nsi.h"

#define NSI_CH NSI_CH_0

static struct input_data input[7] = {0};

static TaskHandle_t nsi_task_handle;
static nsi_frame_t nsi_frame = {0};
static nsi_frame_t nsi_ident = {{0x05, 0x00, 0x00}, 3, 2};
static nsi_frame_t nsi_status = {{0x00, 0x00, 0x00, 0x00}, 4, 2};

static void nsi_wait_for_cmd_task(void *arg) {
    nsi_init(NSI_CH, 26, NSI_SLAVE, arg);

    while (1) {
        printf("Waiting for cmd\n");
        nsi_rx(NSI_CH, &nsi_frame);

        switch (nsi_frame.data[0]) {
            case 0x00:
                printf("IDENTITY\n");
                nsi_tx(NSI_CH, &nsi_ident);
                break;
            case 0x01:
                printf("STATUS\n");
                nsi_tx(NSI_CH, &nsi_status);
                break;
            case 0x02:
                printf("READ\n");
                break;
            case 0x03:
                printf("WRITE\n");
                break;
            case 0xFF:
                printf("RESET\n");
                break;
            default:
                printf("UNKNOWN %02X\n", nsi_frame.data[0]);
                break;
        }
    }
}

void app_main()
{
    xTaskCreatePinnedToCore(nsi_wait_for_cmd_task, "nsi_wait_for_cmd_task",
        2048, &nsi_task_handle, 10, &nsi_task_handle, 1);

    if (bt_init(input)) {
        printf("Bluetooth init fail!\n");
    }
}

