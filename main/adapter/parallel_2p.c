#include <string.h>
#include "config.h"
#include "zephyr/types.h"
#include "util.h"
#include "parallel_2p.h"
#include "driver/gpio.h"

#define P1_TR_PIN 27
#define P1_TL_PIN 26
#define P1_R_PIN 23
#define P1_L_PIN 18
#define P1_D_PIN 5
#define P1_U_PIN 3

#define P2_TR_PIN 16
#define P2_TL_PIN 33
#define P2_R_PIN 25
#define P2_L_PIN 22
#define P2_D_PIN 21
#define P2_U_PIN 19

#define P1_1 P1_TL_PIN
#define P1_2 P1_TR_PIN
#define P1_LD_UP P1_U_PIN
#define P1_LD_DOWN P1_D_PIN
#define P1_LD_LEFT P1_L_PIN
#define P1_LD_RIGHT P1_R_PIN

#define P2_1 P2_TL_PIN
#define P2_2 P2_TR_PIN
#define P2_LD_UP P2_U_PIN
#define P2_LD_DOWN P2_D_PIN
#define P2_LD_LEFT P2_L_PIN
#define P2_LD_RIGHT P2_R_PIN

struct para_2p_map {
    uint32_t buttons;
    uint32_t buttons_high;
} __packed;

static const uint32_t para_2p_mask[4] = {0x00050F00, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t para_2p_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t para_2p_btns_mask[2][32] = {
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(P1_LD_LEFT), BIT(P1_LD_RIGHT), BIT(P1_LD_DOWN), BIT(P1_LD_UP),
        0, 0, 0, 0,
        BIT(P1_1), 0, BIT(P1_2), 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        BIT(P2_LD_LEFT), BIT(P2_LD_RIGHT), BIT(P2_LD_DOWN), BIT(P2_LD_UP),
        0, 0, 0, 0,
        BIT(P2_1 - 32) | 0xF0000000, 0, BIT(P2_2), 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
};

void para_2p_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    struct para_2p_map *map1 = (struct para_2p_map *)wired_adapter.data[0].output;
    struct para_2p_map *map2 = (struct para_2p_map *)wired_adapter.data[1].output;

    map1->buttons = 0xFFFDFFFD;
    map2->buttons = 0xFFFDFFFD;

    map1->buttons_high = 0xFFFFFFFE;
    map2->buttons_high = 0xFFFFFFFE;
}

void para_2p_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        ctrl_data[i].mask = para_2p_mask;
        ctrl_data[i].desc = para_2p_desc;
    }
}

void para_2p_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    if (ctrl_data->index < 2) {
        struct para_2p_map map_tmp;
        struct para_2p_map *map1 = (struct para_2p_map *)wired_adapter.data[0].output;
        struct para_2p_map *map2 = (struct para_2p_map *)wired_adapter.data[1].output;

        memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

        for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
            if (ctrl_data->map_mask[0] & BIT(i)) {
                if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                    if ((para_2p_btns_mask[ctrl_data->index][i] & 0xF0000000) == 0xF0000000) {
                        map_tmp.buttons_high &= ~(para_2p_btns_mask[ctrl_data->index][i] & 0x000000FF);
                    }
                    else {
                        map_tmp.buttons &= ~para_2p_btns_mask[ctrl_data->index][i];
                    }
                }
                else {
                    if ((para_2p_btns_mask[ctrl_data->index][i] & 0xF0000000) == 0xF0000000) {
                        map_tmp.buttons_high |= para_2p_btns_mask[ctrl_data->index][i] & 0x000000FF;
                    }
                    else {
                        map_tmp.buttons |= para_2p_btns_mask[ctrl_data->index][i];
                    }
                }
            }
        }

        GPIO.out = map1->buttons & map2->buttons;
        GPIO.out1.val = map1->buttons_high & map2->buttons_high;

        memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
    }
}
