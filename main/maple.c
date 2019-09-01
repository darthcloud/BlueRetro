#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_intr_alloc.h"
#include "driver/gpio.h"
#include "esp32/dport_access.h"

#define DEBUG  (1ULL << 25)
#define MAPLE0 (1ULL << 26)
#define MAPLE1 (1ULL << 27)
#define TIMEOUT 8

#define delay_ns(mul) for(uint32_t i = 0; i < mul; ++i);
#define delay_100_ns() for(uint32_t i = 0; i < 3; ++i);
#define delay_200_ns() for(uint32_t i = 0; i < 4; ++i);
#define delay_300_ns() for(uint32_t i = 0; i < 6; ++i);
uint32_t intr_cnt = 0;

uint8_t buffer[544] = {0};

static void IRAM_ATTR maple_tx(void) {
    uint8_t *data = buffer;

    ets_delay_us(55);

    GPIO.out_w1ts = MAPLE0 | MAPLE1;
    gpio_set_direction(26, GPIO_MODE_OUTPUT);
    gpio_set_direction(27, GPIO_MODE_OUTPUT);
    DPORT_STALL_OTHER_CPU_START();
    GPIO.out_w1tc = MAPLE0;
    delay_ns(6);
    GPIO.out_w1tc = MAPLE1;
    delay_ns(6);;
    GPIO.out_w1ts = MAPLE1;
    delay_ns(6);
    GPIO.out_w1tc = MAPLE1;
    delay_ns(6);
    GPIO.out_w1ts = MAPLE1;
    delay_ns(6);
    GPIO.out_w1tc = MAPLE1;
    delay_ns(6);
    GPIO.out_w1ts = MAPLE1;
    delay_ns(6);
    GPIO.out_w1tc = MAPLE1;
    delay_ns(6);
    GPIO.out_w1ts = MAPLE1;
    delay_ns(6);
    GPIO.out_w1ts = MAPLE0;

    for (uint32_t bit = 0; bit < 5*8; ++data) {
        for (uint32_t mask = 0x80; mask; mask >>= 1, ++bit) {
            GPIO.out_w1ts = MAPLE0;
            GPIO.out_w1tc = MAPLE1;
            delay_ns(1);
            if (*data & mask) {
                GPIO.out_w1ts = MAPLE1;
            }
            else {
                GPIO.out_w1tc = MAPLE1;
            }
            delay_ns(1);
            GPIO.out_w1tc = MAPLE0;
            delay_ns(2);
            mask >>= 1;
            ++bit;
            GPIO.out_w1tc = MAPLE0;
            GPIO.out_w1ts = MAPLE1;
            delay_ns(1);
            if (*data & mask) {
                GPIO.out_w1ts = MAPLE0;
            }
            else {
                GPIO.out_w1tc = MAPLE0;
            }
            delay_ns(1);
            GPIO.out_w1tc = MAPLE1;
            delay_ns(2);
        }
    }
    GPIO.out_w1ts = MAPLE0;
    GPIO.out_w1tc = MAPLE1;

    delay_ns(6);
    GPIO.out_w1ts = MAPLE1;
    delay_ns(6);
    GPIO.out_w1tc = MAPLE1;
    delay_ns(8);
    GPIO.out_w1tc = MAPLE0;
    delay_ns(6);
    GPIO.out_w1ts = MAPLE0;
    delay_ns(6);
    GPIO.out_w1tc = MAPLE0;
    delay_ns(6);
    GPIO.out_w1ts = MAPLE0;
    delay_ns(6);
    GPIO.out_w1ts = MAPLE1;

    gpio_set_direction(26, GPIO_MODE_INPUT);
    gpio_set_direction(27, GPIO_MODE_INPUT);
    DPORT_STALL_OTHER_CPU_END();
    /* Send start sequence */

}

static void IRAM_ATTR maple_rx(void* arg)
{
    const uint32_t gpio_intr_status = GPIO.acpu_int;
    uint32_t timeout = 0;
    uint32_t bit_cnt = 0;
    uint32_t gpio;
    uint8_t *data = buffer;
    uint32_t byte;

    if (gpio_intr_status) {
        DPORT_STALL_OTHER_CPU_START();
        GPIO.out_w1tc = DEBUG;
        while (1) {
            do {
                while (!(GPIO.in & MAPLE0));
                while (((gpio = GPIO.in) & MAPLE0));
                if (gpio & MAPLE1) {
                    *data = (*data << 1) + 1;
                }
                else {
                    *data <<= 1;
                }
                ++bit_cnt;
                //GPIO.out_w1ts = DEBUG;
                while (!(GPIO.in & MAPLE1));
                timeout = 0;
                while (((gpio = GPIO.in) & MAPLE1)) {
                    if (++timeout > TIMEOUT) {
                        goto maple_end;
                    }
                }
                if (gpio & MAPLE0) {
                    *data = (*data << 1) + 1;
                }
                else {
                    *data <<= 1;
                }
                ++bit_cnt;
                //GPIO.out_w1tc = DEBUG;
            } while ((bit_cnt % 8));
            ++data;
        }
maple_end:
        DPORT_STALL_OTHER_CPU_END();
        GPIO.out_w1ts = DEBUG;
        byte = ((bit_cnt - 1) / 8);
        if ((bit_cnt - 1) % 8) {
            //ets_printf("*");
            ++byte;
        }
        //for(uint16_t i = 0; i < byte; ++i) {
        //    ets_printf("%02X", buffer[i]);
        //}
        //ets_printf("\n");
        maple_tx();

        GPIO.status_w1tc = gpio_intr_status;
    }
}

void init_maple(void)
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
    esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3, maple_rx, NULL, NULL);

    while (1) {
        //printf("JG2019 intr_cnt: %d\n", intr_cnt);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

