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

#ifndef _GPIO_H_
#define _GPIO_H_

#include <hal/gpio_types.h>

extern const uint32_t GPIO_PIN_MUX_REG_IRAM[];

int32_t gpio_set_level_iram(gpio_num_t gpio_num, uint32_t level);
int32_t gpio_set_pull_mode_iram(gpio_num_t gpio_num, gpio_pull_mode_t pull);
int32_t gpio_set_direction_iram(gpio_num_t gpio_num, gpio_mode_t mode);
int32_t gpio_config_iram(const gpio_config_t *pGPIOConfig);
int32_t gpio_reset_iram(gpio_num_t gpio_num);

#endif /* _GPIO_H_ */
