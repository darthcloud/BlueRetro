#include <stdio.h>
#include <string.h>
#include "config.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "parallel_auto.h"
#include "driver/gpio.h"

#define P1_TR_PIN 27
#define P1_TL_PIN 26
#define P1_R_PIN 23
#define P1_L_PIN 18
#define P1_D_PIN 5
#define P1_U_PIN 3

#define P1_1 P1_TL_PIN
#define P1_2 P1_TR_PIN
#define P1_LD_UP P1_U_PIN
#define P1_LD_DOWN P1_D_PIN
#define P1_LD_LEFT P1_L_PIN
#define P1_LD_RIGHT P1_R_PIN

struct para_auto_map {
    uint32_t buttons;
} __packed;

static const uint32_t para_auto_mask[4] = {0x00050F00, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t para_auto_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t para_auto_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(P1_LD_LEFT), BIT(P1_LD_RIGHT), BIT(P1_LD_DOWN), BIT(P1_LD_UP),
    0, 0, 0, 0,
    BIT(P1_1), 0, BIT(P1_2), 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
};

static const uint8_t output_list[] = {
    3, 5, 18, 23, 26, 27
};

static void parallel_io_init(void)
{
    gpio_config_t io_conf = {0};

    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    for (uint32_t i = 0; i < ARRAY_SIZE(output_list); i++) {
        io_conf.pin_bit_mask = 1ULL << output_list[i];
        gpio_config(&io_conf);
        gpio_set_level(output_list[i], 1);
    }
}

void para_auto_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    struct para_auto_map *map = (struct para_auto_map *)wired_data->output;

    map->buttons = 0xFFFDFFFD;
}

void para_auto_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        ctrl_data[i].mask = para_auto_mask;
        ctrl_data[i].desc = para_auto_desc;
    }
}

void para_auto_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    if (ctrl_data->index < 1) {
        struct para_auto_map map_tmp;
        memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

        if (!atomic_test_bit(&wired_data->flags, WIRED_GPIO_INIT)) {
            parallel_io_init();
            atomic_set_bit(&wired_data->flags, WIRED_GPIO_INIT);
        }

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (ctrl_data->map_mask[0] & BIT(i)) {
                if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                    map_tmp.buttons &= ~para_auto_btns_mask[i];
                }
                else {
                    map_tmp.buttons |= para_auto_btns_mask[i];
                }
            }
        }

        GPIO.out = map_tmp.buttons;
        memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
    }
}
