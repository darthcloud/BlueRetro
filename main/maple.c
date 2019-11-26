#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <xtensa/hal.h>
#include <esp32/dport_access.h>
#include <esp_intr_alloc.h>
#include "driver/gpio.h"
#include "zephyr/types.h"
#include "util.h"
#include "adapter.h"
#include "maple.h"

//#define WIRED_TRACE
#define DEBUG  (1ULL << 25)
#define TIMEOUT 8

#define MAPLE_FUNC_DATA_CTRL 0x3FFFFF
#define wait_100ns() asm("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
#define maple_fix_byte(s, a, b) (s ? ((a << s) | (b >> (8 - s))) : b)

const uint8_t gpio_pin[4][2] = {
    {26, 27},
    { 5, 18},
    {19, 25},
    {22, 23},
};

uint8_t pin_to_port[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x02, 0x00, 0x02, 0x03, 0x03,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

uint8_t dev_info[] =
{
    0x1C, 0x20, 0x00, 0x05, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x72, 0x44, 0x00, 0xFF, 0x63, 0x6D, 0x61, 0x65, 0x20, 0x74, 0x73, 0x61,
    0x74, 0x6E, 0x6F, 0x43, 0x6C, 0x6C, 0x6F, 0x72, 0x20, 0x20, 0x72, 0x65, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x64, 0x6F, 0x72, 0x50, 0x64, 0x65, 0x63, 0x75, 0x20, 0x79, 0x42, 0x20,
    0x55, 0x20, 0x72, 0x6F, 0x72, 0x65, 0x64, 0x6E, 0x63, 0x69, 0x4C, 0x20, 0x65, 0x73, 0x6E, 0x65,
    0x6F, 0x72, 0x46, 0x20, 0x45, 0x53, 0x20, 0x6D, 0x45, 0x20, 0x41, 0x47, 0x52, 0x45, 0x54, 0x4E,
    0x53, 0x49, 0x52, 0x50, 0x4C, 0x2C, 0x53, 0x45, 0x20, 0x2E, 0x44, 0x54, 0x20, 0x20, 0x20, 0x20,
    0x01, 0xF4, 0x01, 0xAE, 0x00
};

uint8_t status[] =
{
    0x03, 0x20, 0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80, 0x00
};

uint32_t intr_cnt = 0;

uint8_t buffer[544] = {0};

static void IRAM_ATTR maple_tx(uint32_t port, uint8_t *data, uint8_t len) {
    uint8_t *crc = data + (len - 1);
    uint32_t maple0 = BIT(gpio_pin[port][0]);
    uint32_t maple1 = BIT(gpio_pin[port][1]);

    ets_delay_us(55);

    GPIO.out_w1ts = maple0 | maple1;
    gpio_set_direction(gpio_pin[port][0], GPIO_MODE_OUTPUT);
    gpio_set_direction(gpio_pin[port][1], GPIO_MODE_OUTPUT);
    DPORT_STALL_OTHER_CPU_START();
    GPIO.out_w1tc = maple0;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple1;
    wait_100ns();
    wait_100ns();

    for (uint32_t bit = 0; bit < len*8; ++data) {
        for (uint32_t mask = 0x80; mask; mask >>= 1, ++bit) {
            GPIO.out_w1ts = maple0;
            wait_100ns();
            wait_100ns();
            if (*data & mask) {
                GPIO.out_w1ts = maple1;
            }
            else {
                GPIO.out_w1tc = maple1;
            }
            wait_100ns();
            GPIO.out_w1tc = maple0;
            wait_100ns();
            wait_100ns();
            mask >>= 1;
            ++bit;
            GPIO.out_w1ts = maple1;
            wait_100ns();
            wait_100ns();
            if (*data & mask) {
                GPIO.out_w1ts = maple0;
            }
            else {
                GPIO.out_w1tc = maple0;
            }
            wait_100ns();
            GPIO.out_w1tc = maple1;
            wait_100ns();
            wait_100ns();
        }
        *crc ^= *data;
    }
    GPIO.out_w1ts = maple0;
    wait_100ns();
    GPIO.out_w1ts = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple1;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple0;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple0;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1tc = maple0;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple0;
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    wait_100ns();
    GPIO.out_w1ts = maple1;

    gpio_set_direction(gpio_pin[port][0], GPIO_MODE_INPUT);
    gpio_set_direction(gpio_pin[port][1], GPIO_MODE_INPUT);
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
#ifdef WIRED_TRACE
    uint32_t byte;
#endif
    uint32_t bad_frame;
    uint8_t len, cmd, src, dst;
    uint32_t port;
    uint32_t maple0;
    uint32_t maple1;

    if (gpio_intr_status) {
        DPORT_STALL_OTHER_CPU_START();
        port = pin_to_port[(__builtin_ffs(gpio_intr_status) - 1)];
        maple0 = BIT(gpio_pin[port][0]);
        maple1 = BIT(gpio_pin[port][1]);
        //GPIO.out_w1tc = DEBUG;
        while (1) {
            do {
                while (!(GPIO.in & maple0));
                while (((gpio = GPIO.in) & maple0));
                if (gpio & maple1) {
                    *data = (*data << 1) + 1;
                }
                else {
                    *data <<= 1;
                }
                ++bit_cnt;
                //GPIO.out_w1ts = DEBUG;
                while (!(GPIO.in & maple1));
                timeout = 0;
                while (((gpio = GPIO.in) & maple1)) {
                    if (++timeout > TIMEOUT) {
                        goto maple_end;
                    }
                }
                if (gpio & maple0) {
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
        //GPIO.out_w1ts = DEBUG;
        bad_frame = ((bit_cnt - 1) % 8);

#ifdef WIRED_TRACE
        ets_printf("%08X ", xthal_get_ccount());
        byte = ((bit_cnt - 1) / 8);
        if (bad_frame) {
            ++byte;
            for (uint32_t i = 0; i < byte; ++i) {
                ets_printf("%02X", maple_fix_byte(bad_frame, buffer[i ? i - 1 : 0], buffer[i]));
            }
        }
        else {
            for (uint32_t i = 0; i < byte; ++i) {
                ets_printf("%02X", buffer[i]);
            }
        }
        ets_printf("\n");
#else
        len = ((bit_cnt - 1) / 32) - 1;
        /* Fix up to 7 bits loss */
        if (bad_frame) {
            cmd = maple_fix_byte(bad_frame, buffer[2], buffer[3]);
            src = maple_fix_byte(bad_frame, buffer[1], buffer[2]);
            dst = maple_fix_byte(bad_frame, buffer[0], buffer[1]);
        }
        /* Fix 8 bits loss */
        else if (buffer[0] != len) {
            cmd = buffer[2];
            src = buffer[1];
            dst = buffer[0];
        }
        else {
            cmd = buffer[3];
            src = buffer[2];
            dst = buffer[1];
        }
        switch (cmd) {
            case 0x01:
                dev_info[1] = src;
                dev_info[2] = dst;
                maple_tx(port, dev_info, sizeof(dev_info));
                break;
            case 0x09:
                status[1] = src;
                status[2] = dst;
                memcpy(status + 8, wired_adapter.data[port].output, sizeof(status) - 8);
                maple_tx(port, status, sizeof(status));
                ++wired_adapter.data[port].frame_cnt;
                break;
            default:
                ets_printf("Unsupported cmd: 0x%02X\n", cmd);
                break;
        }
#endif

        GPIO.status_w1tc = gpio_intr_status;
    }
}

void maple_init(void)
{
    gpio_config_t io_conf[4][2] = {0};

    for (uint32_t i = 0; i < ARRAY_SIZE(io_conf); i++) {
        for (uint32_t j = 0; j < ARRAY_SIZE(io_conf[0]); j++) {
            io_conf[i][j].intr_type = j ? 0 : GPIO_PIN_INTR_NEGEDGE;
            io_conf[i][j].pin_bit_mask = BIT(gpio_pin[i][j]);
            io_conf[i][j].mode = GPIO_MODE_INPUT;
            io_conf[i][j].pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf[i][j].pull_up_en = GPIO_PULLUP_DISABLE;
            gpio_config(&io_conf[i][j]);
        }
    }

    wired_adapter.system_id = DC;

    for (uint32_t i = 0; i < 4; i++) {
        adapter_init_buffer(i);
    }
#if 0
    gpio_config_t io_conf2 = {
        .intr_type = 0,
        .pin_bit_mask = DEBUG,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&io_conf2);
    GPIO.out_w1ts = DEBUG;
#endif

    esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3, maple_rx, NULL, NULL);
}
