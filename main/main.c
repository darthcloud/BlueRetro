#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "bt.h"

static struct input_data input[7] = {0};

void app_main()
{
    if (bt_init(input)) {
        printf("Bluetooth init fail!\n");
    }
}

