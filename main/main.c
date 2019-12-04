#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "drivers/sd.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "wired/detect.h"
#include "wired/nsi.h"
#include "wired/maple.h"

typedef void (*wired_init_t)(void);

static const wired_init_t wired_init[] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    nsi_init,
    maple_init,
    NULL,
    nsi_init,
    NULL,
};

static void wired_init_task(void *arg) {
    adapter_init();
    detect_init();

    while (wired_adapter.system_id == WIRED_NONE) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    printf("# Detected system : %d\n", wired_adapter.system_id);

    detect_deinit();
    if (wired_adapter.system_id < WIRED_MAX && wired_init[wired_adapter.system_id]) {
        wired_init[wired_adapter.system_id]();
    }
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

