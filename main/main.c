#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "bt.h"
#include "nsi.h"

#define NSI_CH NSI_CH_0

static struct io output[7] = {0};
static struct config config;

static TaskHandle_t wired_task_handle;

static void wired_init_task(void *arg) {
    nsi_init(NSI_CH, 26, NSI_SLAVE, &output[0]);
    vTaskDelete(wired_task_handle);
}

void app_main()
{
    xTaskCreatePinnedToCore(wired_init_task, "wired_init_task", 2048, &wired_task_handle, 10, &wired_task_handle, 1);

    if (sd_init(&config)) {
        printf("SD init fail!\n");
    }

    if (bt_init(&output[0], &config)) {
        printf("Bluetooth init fail!\n");
    }
}

