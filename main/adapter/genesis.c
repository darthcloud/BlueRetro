#include <string.h>
#include "../zephyr/types.h"
#include "../util.h"
#include "genesis.h"
#include "driver/gpio.h"

#define P1_TH_PIN 35
#define P1_TR_PIN 27
#define P1_TL_PIN 26
#define P1_R_PIN 23
#define P1_L_PIN 18
#define P1_D_PIN 5
#define P1_U_PIN 3

#define P2_TH_PIN 36
#define P2_TR_PIN 16
#define P2_TL_PIN 33
#define P2_R_PIN 25
#define P2_L_PIN 22
#define P2_D_PIN 21
#define P2_U_PIN 19

#define P1_A P1_TL_PIN
#define P1_B P1_TL_PIN
#define P1_C P1_TR_PIN
#define P1_START P1_TR_PIN
#define P1_LD_UP P1_U_PIN
#define P1_LD_DOWN P1_D_PIN
#define P1_LD_LEFT P1_L_PIN
#define P1_LD_RIGHT P1_R_PIN
#define P1_Z P1_U_PIN
#define P1_Y P1_D_PIN
#define P1_X P1_L_PIN
#define P1_MODE P1_R_PIN

#define P2_A P2_TL_PIN
#define P2_B P2_TL_PIN
#define P2_C P2_TR_PIN
#define P2_START P2_TR_PIN
#define P2_LD_UP P2_U_PIN
#define P2_LD_DOWN P2_D_PIN
#define P2_LD_LEFT P2_L_PIN
#define P2_LD_RIGHT P2_R_PIN
#define P2_Z P2_U_PIN
#define P2_Y P2_D_PIN
#define P2_X P2_L_PIN
#define P2_MODE P2_R_PIN

struct genesis_map {
    uint32_t buttons[3];
    uint32_t buttons_high[3];
} __packed;

static const uint32_t genesis_mask[4] = {0x223F0F00, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t genesis_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};

static const uint32_t genesis_btns_mask[2][3][32] = {
    {
        /* TH HIGH */
        {
            0, 0, 0, 0,
            0, 0, 0, 0,
            BIT(P1_LD_LEFT), BIT(P1_LD_RIGHT), BIT(P1_LD_DOWN), BIT(P1_LD_UP),
            0, 0, 0, 0,
            0, BIT(P1_B), 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, BIT(P1_C), 0, 0,
        },
        /* TH LOW */
        {
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, BIT(P1_LD_DOWN), BIT(P1_LD_UP),
            0, 0, 0, 0,
            0, 0, BIT(P1_A), 0,
            BIT(P1_START), 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        },
        /* 6BTNS */
        {
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            BIT(P1_X), 0, 0, BIT(P1_Y),
            0, BIT(P1_MODE), 0, 0,
            0, BIT(P1_Z), 0, 0,
            0, 0, 0, 0,
        },
    },
    {
        /* TH HIGH */
        {
            0, 0, 0, 0,
            0, 0, 0, 0,
            BIT(P2_LD_LEFT), BIT(P2_LD_RIGHT), BIT(P2_LD_DOWN), BIT(P2_LD_UP),
            0, 0, 0, 0,
            0, BIT(P2_B - 32) | 0xF0000000, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, BIT(P2_C), 0, 0,
        },
        {
        /* TH LOW */
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, BIT(P2_LD_DOWN), BIT(P2_LD_UP),
            0, 0, 0, 0,
            0, 0, BIT(P2_A - 32) | 0xF0000000, 0,
            BIT(P2_START), 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        },
        /* 6BTNS */
        {
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            BIT(P2_X), 0, 0, BIT(P2_Y),
            0, BIT(P2_MODE), 0, 0,
            0, BIT(P2_Z), 0, 0,
            0, 0, 0, 0,
        },
    },
};

void genesis_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    struct genesis_map *map = (struct genesis_map *)wired_data->output;
    /* Hackish but wtv */
    struct genesis_map *map2 = (struct genesis_map *)wired_adapter.data[1].output;

    map->buttons[0] = 0xFFFFFFFD;
    map->buttons[1] = 0xFF7BFFFD;
    map2->buttons[0] = 0xFFFFFFFD;
    map2->buttons[1] = 0xFDBFFFFD;

    map->buttons_high[0] = 0xFFFFFFFE;
    map->buttons_high[1] = 0xFFFFFFFE;
    map2->buttons_high[0] = 0xFFFFFFFE;
    map2->buttons_high[1] = 0xFFFFFFFE;
}

void genesis_meta_init(int32_t dev_mode, struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        ctrl_data[i].mask = genesis_mask;
        ctrl_data[i].desc = genesis_desc;
    }
}

void genesis_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct genesis_map map_tmp;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                for (uint32_t j = 0; j < 3; j++) {
                    if ((genesis_btns_mask[ctrl_data->index][j][i] & 0xF0000000) == 0xF0000000) {
                        map_tmp.buttons_high[j] &= ~(genesis_btns_mask[ctrl_data->index][j][i] & 0x000000FF);
                    }
                    else {
                        map_tmp.buttons[j] &= ~genesis_btns_mask[ctrl_data->index][j][i];
                    }
                }
            }
            else {
                for (uint32_t j = 0; j < 3; j++) {
                    if ((genesis_btns_mask[ctrl_data->index][j][i] & 0xF0000000) == 0xF0000000) {
                        map_tmp.buttons_high[j] |= genesis_btns_mask[ctrl_data->index][j][i] & 0x000000FF;
                    }
                    else {
                        map_tmp.buttons[j] |= genesis_btns_mask[ctrl_data->index][j][i];
                    }
                }
            }
        }
    }

    if (ctrl_data->index < 2) {
        struct genesis_map *map1 = (struct genesis_map *)wired_adapter.data[0].output;
        struct genesis_map *map2 = (struct genesis_map *)wired_adapter.data[1].output;
        uint32_t cycle = !(GPIO.in1.val & BIT(P1_TH_PIN - 32));
        uint32_t cycle2 = !(GPIO.in1.val & BIT(P2_TH_PIN - 32));
        GPIO.out = map1->buttons[cycle] & map2->buttons[cycle2];
        GPIO.out1.val = map1->buttons_high[cycle] & map2->buttons_high[cycle2];
    }
    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}
