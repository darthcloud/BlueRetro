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
#include "soc/rtc_io_periph.h"
#include "gpio.h"

typedef struct {
    uint32_t reg;       /*!< Register of RTC pad, or 0 if not an RTC GPIO */
    uint32_t pullup;    /*!< Mask of pullup enable */
    uint32_t pulldown;  /*!< Mask of pulldown enable */
} rtc_io_desc_lite_t;

const int rtc_io_num_map_iram[SOC_GPIO_PIN_COUNT] = {
    RTCIO_GPIO0_CHANNEL,    //GPIO0
    -1,//GPIO1
    RTCIO_GPIO2_CHANNEL,    //GPIO2
    -1,//GPIO3
    RTCIO_GPIO4_CHANNEL,    //GPIO4
    -1,//GPIO5
    -1,//GPIO6
    -1,//GPIO7
    -1,//GPIO8
    -1,//GPIO9
    -1,//GPIO10
    -1,//GPIO11
    RTCIO_GPIO12_CHANNEL,   //GPIO12
    RTCIO_GPIO13_CHANNEL,   //GPIO13
    RTCIO_GPIO14_CHANNEL,   //GPIO14
    RTCIO_GPIO15_CHANNEL,   //GPIO15
    -1,//GPIO16
    -1,//GPIO17
    -1,//GPIO18
    -1,//GPIO19
    -1,//GPIO20
    -1,//GPIO21
    -1,//GPIO22
    -1,//GPIO23
    -1,//GPIO24
    RTCIO_GPIO25_CHANNEL,   //GPIO25
    RTCIO_GPIO26_CHANNEL,   //GPIO26
    RTCIO_GPIO27_CHANNEL,   //GPIO27
    -1,//GPIO28
    -1,//GPIO29
    -1,//GPIO30
    -1,//GPIO31
    RTCIO_GPIO32_CHANNEL,   //GPIO32
    RTCIO_GPIO33_CHANNEL,   //GPIO33
    RTCIO_GPIO34_CHANNEL,   //GPIO34
    RTCIO_GPIO35_CHANNEL,   //GPIO35
    RTCIO_GPIO36_CHANNEL,   //GPIO36
    RTCIO_GPIO37_CHANNEL,   //GPIO37
    RTCIO_GPIO38_CHANNEL,   //GPIO38
    RTCIO_GPIO39_CHANNEL,   //GPIO39
};

const rtc_io_desc_lite_t rtc_io_desc_iram[SOC_RTCIO_PIN_COUNT] = {
    /*REG                     Pullup                   Pulldown */
    {RTC_IO_SENSOR_PADS_REG,  0,                       0,                       }, //36
    {RTC_IO_SENSOR_PADS_REG,  0,                       0,                       }, //37
    {RTC_IO_SENSOR_PADS_REG,  0,                       0,                       }, //38
    {RTC_IO_SENSOR_PADS_REG,  0,                       0,                       }, //39
    {RTC_IO_ADC_PAD_REG,      0,                       0,                       }, //34
    {RTC_IO_ADC_PAD_REG,      0,                       0,                       }, //35
    {RTC_IO_PAD_DAC1_REG,     RTC_IO_PDAC1_RUE_M,      RTC_IO_PDAC1_RDE_M,      }, //25
    {RTC_IO_PAD_DAC2_REG,     RTC_IO_PDAC2_RUE_M,      RTC_IO_PDAC2_RDE_M,      }, //26
    {RTC_IO_XTAL_32K_PAD_REG, RTC_IO_X32N_RUE_M,       RTC_IO_X32N_RDE_M,       }, //33
    {RTC_IO_XTAL_32K_PAD_REG, RTC_IO_X32P_RUE_M,       RTC_IO_X32P_RDE_M,       }, //32
    {RTC_IO_TOUCH_PAD0_REG,   RTC_IO_TOUCH_PAD0_RUE_M, RTC_IO_TOUCH_PAD0_RDE_M, }, // 4
    {RTC_IO_TOUCH_PAD1_REG,   RTC_IO_TOUCH_PAD1_RUE_M, RTC_IO_TOUCH_PAD1_RDE_M, }, // 0
    {RTC_IO_TOUCH_PAD2_REG,   RTC_IO_TOUCH_PAD2_RUE_M, RTC_IO_TOUCH_PAD2_RDE_M, }, // 2
    {RTC_IO_TOUCH_PAD3_REG,   RTC_IO_TOUCH_PAD3_RUE_M, RTC_IO_TOUCH_PAD3_RDE_M, }, //15
    {RTC_IO_TOUCH_PAD4_REG,   RTC_IO_TOUCH_PAD4_RUE_M, RTC_IO_TOUCH_PAD4_RDE_M, }, //13
    {RTC_IO_TOUCH_PAD5_REG,   RTC_IO_TOUCH_PAD5_RUE_M, RTC_IO_TOUCH_PAD5_RDE_M, }, //12
    {RTC_IO_TOUCH_PAD6_REG,   RTC_IO_TOUCH_PAD6_RUE_M, RTC_IO_TOUCH_PAD6_RDE_M, }, //14
    {RTC_IO_TOUCH_PAD7_REG,   RTC_IO_TOUCH_PAD7_RUE_M, RTC_IO_TOUCH_PAD7_RDE_M, }, //27
};

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

static inline bool rtc_gpio_is_valid_gpio(gpio_num_t gpio_num)
{
    return (gpio_num < GPIO_PIN_COUNT && rtc_io_num_map_iram[gpio_num] >= 0);
}

static inline void rtcio_ll_pullup_enable(int rtcio_num)
{
    if (rtc_io_desc_iram[rtcio_num].pullup) {
        SET_PERI_REG_MASK(rtc_io_desc_iram[rtcio_num].reg, rtc_io_desc_iram[rtcio_num].pullup);
    }
}

static inline void rtcio_ll_pullup_disable(int rtcio_num)
{
    if (rtc_io_desc_iram[rtcio_num].pullup) {
        CLEAR_PERI_REG_MASK(rtc_io_desc_iram[rtcio_num].reg, rtc_io_desc_iram[rtcio_num].pullup);
    }
}

static inline void rtcio_ll_pulldown_enable(int rtcio_num)
{
    if (rtc_io_desc_iram[rtcio_num].pulldown) {
        SET_PERI_REG_MASK(rtc_io_desc_iram[rtcio_num].reg, rtc_io_desc_iram[rtcio_num].pulldown);
    }
}

static inline void rtcio_ll_pulldown_disable(int rtcio_num)
{
    if (rtc_io_desc_iram[rtcio_num].pulldown) {
        CLEAR_PERI_REG_MASK(rtc_io_desc_iram[rtcio_num].reg, rtc_io_desc_iram[rtcio_num].pulldown);
    }
}

static int32_t gpio_pullup_en_iram(gpio_num_t gpio_num)
{
    if (!rtc_gpio_is_valid_gpio(gpio_num)) {
        uint32_t io_reg = GPIO_PIN_MUX_REG_IRAM[gpio_num];
        REG_SET_BIT(io_reg, FUN_PU);
    } else {
        rtcio_ll_pullup_enable(rtc_io_num_map_iram[gpio_num]);
    }

    return ESP_OK;
}

static int32_t gpio_pullup_dis_iram(gpio_num_t gpio_num)
{
    if (!rtc_gpio_is_valid_gpio(gpio_num)) {
        uint32_t io_reg = GPIO_PIN_MUX_REG_IRAM[gpio_num];
        REG_CLR_BIT(io_reg, FUN_PU);
    } else {
        rtcio_ll_pullup_disable(rtc_io_num_map_iram[gpio_num]);
    }

    return ESP_OK;
}

static int32_t gpio_pulldown_en_iram(gpio_num_t gpio_num)
{
    if (!rtc_gpio_is_valid_gpio(gpio_num)) {
        uint32_t io_reg = GPIO_PIN_MUX_REG_IRAM[gpio_num];
        REG_SET_BIT(io_reg, FUN_PD);
    } else {
        rtcio_ll_pulldown_enable(rtc_io_num_map_iram[gpio_num]);
    }

    return ESP_OK;
}

static int32_t gpio_pulldown_dis_iram(gpio_num_t gpio_num)
{
    if (!rtc_gpio_is_valid_gpio(gpio_num)) {
        uint32_t io_reg = GPIO_PIN_MUX_REG_IRAM[gpio_num];
        REG_CLR_BIT(io_reg, FUN_PD);
    } else {
        rtcio_ll_pulldown_disable(rtc_io_num_map_iram[gpio_num]);
    }

    return ESP_OK;
}

int32_t gpio_set_level_iram(gpio_num_t gpio_num, uint32_t level)
{
    gpio_ll_set_level(&GPIO, gpio_num, level);
    return ESP_OK;
}

int32_t gpio_set_pull_mode_iram(gpio_num_t gpio_num, gpio_pull_mode_t pull)
{
    int32_t ret = ESP_OK;

    switch (pull) {
        case GPIO_PULLUP_ONLY:
            gpio_pulldown_dis_iram(gpio_num);
            gpio_pullup_en_iram(gpio_num);
            break;

        case GPIO_PULLDOWN_ONLY:
            gpio_pulldown_en_iram(gpio_num);
            gpio_pullup_dis_iram(gpio_num);
            break;

        case GPIO_PULLUP_PULLDOWN:
            gpio_pulldown_en_iram(gpio_num);
            gpio_pullup_en_iram(gpio_num);
            break;

        case GPIO_FLOATING:
            gpio_pulldown_dis_iram(gpio_num);
            gpio_pullup_dis_iram(gpio_num);
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
        ets_printf("# GPIO_PIN mask error\n");
        return ESP_ERR_INVALID_ARG;
    }

    if (pGPIOConfig->mode & GPIO_MODE_DEF_OUTPUT &&
        pGPIOConfig->pin_bit_mask & ~SOC_GPIO_VALID_OUTPUT_GPIO_MASK) {
        ets_printf("# GPIO can only be used as input mode\n");
        return ESP_ERR_INVALID_ARG;
    }

    do {
        io_reg = GPIO_PIN_MUX_REG_IRAM[io_num];

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
                gpio_pullup_en_iram(io_num);
            } else {
                gpio_pullup_dis_iram(io_num);
            }

            if (pGPIOConfig->pull_down_en) {
                pd_en = 1;
                gpio_pulldown_en_iram(io_num);
            } else {
                gpio_pulldown_dis_iram(io_num);
            }

            ets_printf("# GPIO[%d]| InputEn: %d| OutputEn: %d| OpenDrain: %d| Pullup: %d| Pulldown: %d| Intr:%d\n",
                io_num, input_en, output_en, od_en, pu_en, pd_en, pGPIOConfig->intr_type);
            gpio_ll_set_intr_type(&GPIO, io_num, pGPIOConfig->intr_type);

            if (pGPIOConfig->intr_type) {
                if (io_num < 32) {
                    gpio_ll_clear_intr_status(&GPIO, BIT(io_num));
                } else {
                    gpio_ll_clear_intr_status_high(&GPIO, BIT(io_num - 32));
                }
                /* Always set interrupt to core 1 */
                gpio_ll_intr_enable_on_core(&GPIO, 1, io_num);
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
