// Copyright 2015-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdbool.h>
#include "hal/cpu_ll.h"
#include "esp_err.h"
#include "esp32/rom/ets_sys.h"
#include "esp_attr.h"
#include "esp_bit_defs.h"
#include "hal/gpio_ll.h"
#include "gpio.h"

#define GPIO_PIN_COUNT (40)

const uint32_t GPIO_PIN_MUX_REG_IRAM[SOC_GPIO_PIN_COUNT] = {
    IO_MUX_GPIO0_REG,
    IO_MUX_GPIO1_REG,
    IO_MUX_GPIO2_REG,
    IO_MUX_GPIO3_REG,
    IO_MUX_GPIO4_REG,
    IO_MUX_GPIO5_REG,
    IO_MUX_GPIO6_REG,
    IO_MUX_GPIO7_REG,
    IO_MUX_GPIO8_REG,
    IO_MUX_GPIO9_REG,
    IO_MUX_GPIO10_REG,
    IO_MUX_GPIO11_REG,
    IO_MUX_GPIO12_REG,
    IO_MUX_GPIO13_REG,
    IO_MUX_GPIO14_REG,
    IO_MUX_GPIO15_REG,
    IO_MUX_GPIO16_REG,
    IO_MUX_GPIO17_REG,
    IO_MUX_GPIO18_REG,
    IO_MUX_GPIO19_REG,
    0,
    IO_MUX_GPIO21_REG,
    IO_MUX_GPIO22_REG,
    IO_MUX_GPIO23_REG,
    0,
    IO_MUX_GPIO25_REG,
    IO_MUX_GPIO26_REG,
    IO_MUX_GPIO27_REG,
    0,
    0,
    0,
    0,
    IO_MUX_GPIO32_REG,
    IO_MUX_GPIO33_REG,
    IO_MUX_GPIO34_REG,
    IO_MUX_GPIO35_REG,
    IO_MUX_GPIO36_REG,
    IO_MUX_GPIO37_REG,
    IO_MUX_GPIO38_REG,
    IO_MUX_GPIO39_REG,
};

int32_t gpio_set_level_iram(gpio_num_t gpio_num, uint32_t level)
{
    gpio_ll_set_level(&GPIO, gpio_num, level);
    return ESP_OK;
}

int32_t gpio_set_pull_mode_iram(gpio_num_t gpio_num, gpio_pull_mode_t pull)
{
    uint32_t io_reg = GPIO_PIN_MUX_REG_IRAM[gpio_num];
    esp_err_t ret = ESP_OK;

    switch (pull) {
        case GPIO_PULLUP_ONLY:
            REG_CLR_BIT(io_reg, FUN_PD);
            REG_SET_BIT(io_reg, FUN_PU);
            break;

        case GPIO_PULLDOWN_ONLY:
            REG_SET_BIT(io_reg, FUN_PD);
            REG_CLR_BIT(io_reg, FUN_PU);
            break;

        case GPIO_PULLUP_PULLDOWN:
            REG_SET_BIT(io_reg, FUN_PD);
            REG_SET_BIT(io_reg, FUN_PU);
            break;

        case GPIO_FLOATING:
            REG_CLR_BIT(io_reg, FUN_PD);
            REG_CLR_BIT(io_reg, FUN_PU);
            break;

        default:
            ret = ESP_ERR_INVALID_ARG;
            break;
    }

    return ret;
}

int32_t gpio_set_direction_iram(gpio_num_t gpio_num, gpio_mode_t mode)
{
    uint32_t io_reg = GPIO_PIN_MUX_REG_IRAM[gpio_num];

    if (mode & GPIO_MODE_DEF_INPUT) {
        PIN_INPUT_ENABLE(io_reg);
    } else {
        PIN_INPUT_DISABLE(io_reg);
    }

    if (mode & GPIO_MODE_DEF_OUTPUT) {
        gpio_ll_output_enable(&GPIO, gpio_num);
    } else {
        gpio_ll_output_disable(&GPIO, gpio_num);
    }

    if (mode & GPIO_MODE_DEF_OD) {
        gpio_ll_od_enable(&GPIO, gpio_num);
    } else {
        gpio_ll_od_disable(&GPIO, gpio_num);
    }

    return ESP_OK;
}

int32_t gpio_config_iram(const gpio_config_t *pGPIOConfig)
{
    uint64_t gpio_pin_mask = (pGPIOConfig->pin_bit_mask);
    uint32_t io_reg = 0;
    uint32_t io_num = 0;
    uint8_t input_en = 0;
    uint8_t output_en = 0;
    uint8_t od_en = 0;
    uint8_t pu_en = 0;
    uint8_t pd_en = 0;

    if (pGPIOConfig->pin_bit_mask == 0 ||
        pGPIOConfig->pin_bit_mask & ~SOC_GPIO_VALID_GPIO_MASK) {
        ets_printf("GPIO_PIN mask error\n");
        return ESP_ERR_INVALID_ARG;
    }

    if (pGPIOConfig->mode & GPIO_MODE_DEF_OUTPUT &&
        pGPIOConfig->pin_bit_mask & ~SOC_GPIO_VALID_OUTPUT_GPIO_MASK) {
        ets_printf("GPIO can only be used as input mode\n");
        return ESP_ERR_INVALID_ARG;
    }

    do {
        io_reg = GPIO_PIN_MUX_REG_IRAM[io_num];
        uint32_t core_id = cpu_ll_get_core_id();

        if (((gpio_pin_mask >> io_num) & BIT(0))) {
            assert(io_reg != (intptr_t)NULL);

            if ((pGPIOConfig->mode) & GPIO_MODE_DEF_INPUT) {
                input_en = 1;
                PIN_INPUT_ENABLE(io_reg);
            } else {
                PIN_INPUT_DISABLE(io_reg);
            }

            if ((pGPIOConfig->mode) & GPIO_MODE_DEF_OD) {
                od_en = 1;
                gpio_ll_od_enable(&GPIO, io_num);
            } else {
                gpio_ll_od_disable(&GPIO, io_num);
            }

            if ((pGPIOConfig->mode) & GPIO_MODE_DEF_OUTPUT) {
                output_en = 1;
                gpio_ll_output_enable(&GPIO, io_num);
            } else {
                gpio_ll_output_disable(&GPIO, io_num);
            }

            if (pGPIOConfig->pull_up_en) {
                pu_en = 1;
                REG_SET_BIT(io_reg, FUN_PU);
            } else {
                REG_CLR_BIT(io_reg, FUN_PU);
            }

            if (pGPIOConfig->pull_down_en) {
                pd_en = 1;
                REG_SET_BIT(io_reg, FUN_PD);
            } else {
                REG_CLR_BIT(io_reg, FUN_PD);
            }

            ets_printf("GPIO[%d]| InputEn: %d| OutputEn: %d| OpenDrain: %d| Pullup: %d| Pulldown: %d| Intr:%d\n",
                io_num, input_en, output_en, od_en, pu_en, pd_en, pGPIOConfig->intr_type);
            gpio_ll_set_intr_type(&GPIO, io_num, pGPIOConfig->intr_type);

            if (pGPIOConfig->intr_type) {
                if (io_num < 32) {
                    gpio_ll_clear_intr_status(&GPIO, BIT(io_num));
                } else {
                    gpio_ll_clear_intr_status_high(&GPIO, BIT(io_num - 32));
                }
                gpio_ll_intr_enable_on_core(&GPIO, core_id, io_num);
            } else {
                gpio_ll_intr_disable(&GPIO, io_num);
                if (io_num < 32) {
                    gpio_ll_clear_intr_status(&GPIO, BIT(io_num));
                } else {
                    gpio_ll_clear_intr_status_high(&GPIO, BIT(io_num - 32));
                }
            }

            PIN_FUNC_SELECT(io_reg, PIN_FUNC_GPIO); /*function number 2 is GPIO_FUNC for each pin */
        }

        io_num++;
    } while (io_num < GPIO_PIN_COUNT);

    return ESP_OK;
}

int32_t gpio_reset_iram(gpio_num_t gpio_num) {
    gpio_config_t cfg = {
        .pin_bit_mask = BIT64(gpio_num),
        .mode = GPIO_MODE_DISABLE,
        //for powersave reasons, the GPIO should not be floating, select pullup
        .pull_up_en = true,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config_iram(&cfg);
}
