#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "bt.h"
#include "nsi.h"
#include "sd.h"

#define NSI_CH NSI_CH_0

static struct io output[7] = {0};

static TaskHandle_t nsi_task_handle;

static void nsi_wait_for_cmd_task(void *arg) {
    nsi_init(NSI_CH, 26, NSI_SLAVE, &output[0]);
    vTaskDelete(nsi_task_handle);
}

void app_main()
{
    xTaskCreatePinnedToCore(nsi_wait_for_cmd_task, "nsi_wait_for_cmd_task",
        2048, &nsi_task_handle, 10, &nsi_task_handle, 1);

    sd_init();

    if (bt_init(&output[0])) {
        printf("Bluetooth init fail!\n");
    }
}

