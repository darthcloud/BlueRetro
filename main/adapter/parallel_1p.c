#include <string.h>
#include "config.h"
#include "../zephyr/types.h"
#include "../util.h"
#include "parallel_1p.h"
#include "driver/gpio.h"

#define P1_LD_UP 3
#define P1_LD_DOWN 5
#define P1_LD_LEFT 18
#define P1_LD_RIGHT 23
#define P1_A 26
#define P1_B 27
#define P1_C 19
#define P1_D 21
#define P1_SELECT 22
#define P1_START 32
#define P1_CREDIT 33
#define P1_6 25

struct para_1p_map {
    uint32_t buttons;
    uint32_t buttons_high;
} __packed;

static const uint32_t para_1p_mask[4] = {0x017F0F00, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t para_1p_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t para_1p_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(P1_LD_LEFT), BIT(P1_LD_RIGHT), BIT(P1_LD_DOWN), BIT(P1_LD_UP),
    0, 0, 0, 0,
    BIT(P1_C), BIT(P1_B), BIT(P1_A), BIT(P1_D),
    BIT(P1_START - 32) | 0xF0000000, BIT(P1_SELECT), BIT(P1_CREDIT - 32) | 0xF0000000, 0,
    BIT(P1_6), 0, 0, 0,
    0, 0, 0, 0,
};

void para_1p_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    struct para_1p_map *map = (struct para_1p_map *)wired_data->output;

    map->buttons = 0xFFFDFFFF;
    map->buttons_high = 0xFFFFFFFF;
}

void para_1p_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        ctrl_data[i].mask = para_1p_mask;
        ctrl_data[i].desc = para_1p_desc;
    }
}

void para_1p_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    if (ctrl_data->index < 1) {
        struct para_1p_map map_tmp;

        memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (ctrl_data->map_mask[0] & BIT(i)) {
                if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                    if ((para_1p_btns_mask[i] & 0xF0000000) == 0xF0000000) {
                        map_tmp.buttons_high &= ~(para_1p_btns_mask[i] & 0x000000FF);
                    }
                    else {
                        map_tmp.buttons &= ~para_1p_btns_mask[i];
                    }
                }
                else {
                    if ((para_1p_btns_mask[i] & 0xF0000000) == 0xF0000000) {
                        map_tmp.buttons_high |= para_1p_btns_mask[i] & 0x000000FF;
                    }
                    else {
                        map_tmp.buttons |= para_1p_btns_mask[i];
                    }
                }
            }
        }

        GPIO.out = map_tmp.buttons;
        GPIO.out1.val = map_tmp.buttons_high;

        memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
    }
}
