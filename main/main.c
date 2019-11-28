#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "drivers/sd.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "wired/nsi.h"
#include "wired/maple.h"

static void wired_init_task(void *arg) {
    adapter_init();
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

