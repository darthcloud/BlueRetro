#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "bt.h"
#include "nsi.h"
#include "bitbang.h"

#define NSI_CH NSI_CH_0

static struct io output[7] = {0};
static struct config config;

static TaskHandle_t wired_task_handle;

static void wired_init_task(void *arg) {
    //nsi_init(NSI_CH, 26, NSI_SLAVE, &output[0]);
    init_bitbang();
    vTaskDelete(wired_task_handle);
}

static void wl_init_task(void *arg) {
#if 0
    if (sd_init(&config)) {
        printf("SD init fail!\n");
    }
#endif

    if (bt_init(&output[0], &config)) {
        printf("Bluetooth init fail!\n");
    }
    vTaskDelete(NULL);
}

void app_main()
{
    xTaskCreatePinnedToCore(wl_init_task, "wl_init_task", 2048, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(wired_init_task, "wired_init_task", 2048, &wired_task_handle, 10, &wired_task_handle, 1);
}

