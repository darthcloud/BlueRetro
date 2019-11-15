#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "sd.h"
#include "config.h"
#include "bt_host.h"
#include "nsi.h"
#include "maple.h"

static void wired_init_task(void *arg) {
    //nsi_init();
    maple_init();
    vTaskDelete(NULL);
}

static void wl_init_task(void *arg) {
    if (sd_init()) {
        printf("SD init fail!\n");
    }

    config_init();

    if (bt_host_init()) {
        printf("Bluetooth init fail!\n");
    }
    vTaskDelete(NULL);
}

void app_main()
{
    xTaskCreatePinnedToCore(wl_init_task, "wl_init_task", 2048, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(wired_init_task, "wired_init_task", 2048, NULL, 10, NULL, 1);
}

