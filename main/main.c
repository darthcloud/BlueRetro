#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "bt.h"
#include "nsi.h"

#define NSI_CH NSI_CH_0

static struct io input[7] = {0};

static TaskHandle_t nsi_task_handle;

static void nsi_wait_for_cmd_task(void *arg) {
    nsi_init(NSI_CH, 26, NSI_SLAVE, arg);
    vTaskDelete(nsi_task_handle);
}

void app_main()
{
    xTaskCreatePinnedToCore(nsi_wait_for_cmd_task, "nsi_wait_for_cmd_task",
        2048, &nsi_task_handle, 10, &nsi_task_handle, 1);

    //if (bt_init(input)) {
    //    printf("Bluetooth init fail!\n");
    //}
}

