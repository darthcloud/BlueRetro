#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "bt.h"

void app_main()
{
    if (bt_init()) {
        printf("Bluetooth init fail!\n");
    }
}

