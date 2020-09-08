#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "drivers/sd.h"
#include "drivers/led.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "wired/detect.h"
#include "wired/npiso.h"
#include "wired/sega_io.h"
#include "wired/nsi.h"
#include "wired/maple.h"

typedef void (*wired_init_t)(void);

static const char *sys_name[WIRED_MAX] = {
    "AUTO",
    "NES",
    "SMS",
    "PCE",
    "MD-GEN",
    "SNES",
    "PSX",
    "SATURN",
    "N64",
    "DC",
    "PS2",
    "GC",
    "Wii-EXT",
};

static const wired_init_t wired_init[WIRED_MAX] = {
    NULL,
    npiso_init,
    NULL,
    NULL,
    sega_io_init,
    npiso_init,
    NULL,
    sega_io_init,
    nsi_init,
    maple_init,
    NULL,
    nsi_init,
    NULL,
};

static void wired_init_task(void *arg) {
    adapter_init();

#if 1
    detect_init();
    while (wired_adapter.system_id == WIRED_NONE) {
        if (config.magic == CONFIG_MAGIC && config.global_cfg.system_cfg < WIRED_MAX
            && config.global_cfg.system_cfg != WIRED_AUTO) {
            break;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    detect_deinit();
#else
    wired_adapter.system_id = DC;
#endif

    if (wired_adapter.system_id >= 0) {
        printf("# Detected system : %d: %s\n", wired_adapter.system_id, sys_name[wired_adapter.system_id]);
    }

    while (config.magic != CONFIG_MAGIC) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    if (config.global_cfg.system_cfg < WIRED_MAX && config.global_cfg.system_cfg != WIRED_AUTO) {
        wired_adapter.system_id = config.global_cfg.system_cfg;
        printf("# Config override system : %d: %s\n", wired_adapter.system_id, sys_name[wired_adapter.system_id]);
    }

    for (uint32_t i = 0; i < WIRED_MAX; i++) {
        adapter_init_buffer(i);
    }

    if (wired_adapter.system_id < WIRED_MAX && wired_init[wired_adapter.system_id]) {
        wired_init[wired_adapter.system_id]();
    }
    vTaskDelete(NULL);
}

static void wl_init_task(void *arg) {
    err_led_clear();

    if (sd_init()) {
        err_led_set();
        printf("SD init fail!\n");
    }

    config_init();

    if (bt_host_init()) {
        err_led_set();
        printf("Bluetooth init fail!\n");
    }
    vTaskDelete(NULL);
}

void app_main()
{
    xTaskCreatePinnedToCore(wl_init_task, "wl_init_task", 2048, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(wired_init_task, "wired_init_task", 2048, NULL, 10, NULL, 1);
}

