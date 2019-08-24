#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_intr_alloc.h"
#include "driver/gpio.h"

#define DEBUG  (1ULL << 25)
#define MAPLE0 (1ULL << 26)
#define MAPLE1 (1ULL << 27)
#define TIMEOUT 14
uint32_t intr_cnt = 0;

static void IRAM_ATTR maple_rx(void* arg)
{
    const uint32_t gpio_intr_status = GPIO.acpu_int;
    uint32_t timeout = 0;
    if (gpio_intr_status) {
        while (1) {
            while (!(GPIO.in & MAPLE0)) {
                if (++timeout > TIMEOUT) {
                    goto maple_end;
                }
            };
            timeout = 0;
            while ((GPIO.in & MAPLE0)) {
                if (++timeout > TIMEOUT) {
                    goto maple_end;
                }
            }
            timeout = 0;
            while (!(GPIO.in & MAPLE1)) {
                if (++timeout > TIMEOUT) {
                    goto maple_end;
                }
            }
            timeout = 0;
            while ((GPIO.in & MAPLE1)) {
                if (++timeout > TIMEOUT) {
                    goto maple_end;
                }
            }
            timeout = 0;
        }
maple_end:
        GPIO.out_w1tc = DEBUG;
        GPIO.status_w1tc = gpio_intr_status;
    }
    GPIO.out_w1ts = DEBUG;
}

void init_bitbang(void)
{
    gpio_config_t io_conf0 = {
        .intr_type = GPIO_PIN_INTR_NEGEDGE,
        .pin_bit_mask = MAPLE0,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };

    gpio_config_t io_conf1 = {
        .intr_type = 0,
        .pin_bit_mask = MAPLE1,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };

    gpio_config_t io_conf2 = {
        .intr_type = 0,
        .pin_bit_mask = DEBUG,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };

    gpio_config(&io_conf0);
    gpio_config(&io_conf1);
    gpio_config(&io_conf2);
    GPIO.out_w1ts = DEBUG;

    while (!((GPIO.in & (MAPLE0 | MAPLE1)) == (MAPLE0 | MAPLE1)));
    esp_intr_alloc(ETS_GPIO_INTR_SOURCE, 0, maple_rx, NULL, NULL);

    while (1) {
        //printf("JG2019 intr_cnt: %d\n", intr_cnt);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

