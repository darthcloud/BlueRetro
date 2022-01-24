#include <string.h>
#include "adapter/config.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "jag.h"
#include "driver/gpio.h"
#include "wired/jag_io.h"

#define P1_J0_PIN 32
#define P1_J1_PIN 33
#define P1_J2_PIN 35
#define P1_J3_PIN 36
#define P1_J8_PIN 18
#define P1_J9_PIN 19
#define P1_J10_PIN 21
#define P1_J11_PIN 22
#define P1_B0_PIN 25
#define P1_B1_PIN 23

#define P1_J0_MASK (1 << (P1_J0_PIN - 32))
#define P1_J1_MASK (1 << (P1_J1_PIN - 32))
#define P1_J2_MASK (1 << (P1_J2_PIN - 32))
#define P1_J3_MASK (1 << (P1_J3_PIN - 32))
#define P1_J8_MASK (1 << P1_J8_PIN)
#define P1_J9_MASK (1 << P1_J9_PIN)
#define P1_J10_MASK (1 << P1_J10_PIN)
#define P1_J11_MASK (1 << P1_J11_PIN)
#define P1_B0_MASK (1 << P1_B0_PIN)
#define P1_B1_MASK (1 << P1_B1_PIN)

#define NIBBLE_MASK (P1_J8_MASK | P1_J9_MASK | P1_J10_MASK | P1_J11_MASK)

struct jag_6d_axes_idx {
    uint8_t bank_lo;
    uint8_t bank_hi;
    uint8_t row_lo;
    uint8_t row_hi;
};

static DRAM_ATTR const struct jag_6d_axes_idx jag_6d_axes_idx[ADAPTER_MAX_AXES] =
{
    {.bank_lo = 0, .bank_hi = 0, .row_lo = 0, .row_hi = 3},
    {.bank_lo = 0, .bank_hi = 1, .row_lo = 1, .row_hi = 3},
    {.bank_lo = 1, .bank_hi = 2, .row_lo = 0, .row_hi = 0},
    {.bank_lo = 1, .bank_hi = 2, .row_lo = 1, .row_hi = 1},
    {.bank_lo = 0, .bank_hi = 2, .row_lo = 2, .row_hi = 3},
    {.bank_lo = 1, .bank_hi = 2, .row_lo = 2, .row_hi = 2},
};

static DRAM_ATTR const struct ctrl_meta jag_6d_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 128},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 255},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 255},
};

struct jag_map {
    uint32_t buttons[4];
    uint32_t buttons_s1[3][4];
} __packed;

static const uint32_t jag_mask[4] = {0xEEFF7F00, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t jag_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t jag_btns_mask[][32] = {
    /* Pause, A, Up, Down, Left, Right */
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        P1_J10_MASK, P1_J11_MASK, P1_J9_MASK, P1_J8_MASK,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, P1_B0_MASK, 0, 0,
        0, 0, 0, 0,
        0, P1_B1_MASK, 0, 0,
    },
    /* B, *, 7, 4, 1 */
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        P1_J8_MASK, 0, 0, 0,
        P1_J11_MASK, P1_B1_MASK, 0, 0,
        0, 0, 0, 0,
        0, 0, P1_J9_MASK, P1_J10_MASK,
        0, 0, 0, 0,
    },
    /* C, 0, 8, 5, 2 */
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, P1_J8_MASK, 0, 0,
        0, 0, P1_B1_MASK, P1_J11_MASK,
        0, 0, P1_J10_MASK, P1_J9_MASK,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    /* Options, #, 9, 6, 3 */
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, P1_J8_MASK, 0,
        0, 0, 0, 0,
        P1_B1_MASK, 0, 0, 0,
        0, P1_J11_MASK, 0, 0,
        0, 0, P1_J9_MASK, P1_J10_MASK,
    },
};

static const uint32_t jag_6d_mask[4] = {0xFFFF7FFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t jag_6d_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};
static const uint8_t jag_6d_btns_idx[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0x92, 0x81, 0x82, 0x93,
    0, 0, 0x91, 0,
    0, 0x83, 0, 0,
    0, 0x80, 0, 0,
};
static const uint32_t jag_6d_nibble_mask[4] = {
    P1_J8_MASK, P1_J9_MASK, P1_J10_MASK, P1_J11_MASK
};
static DRAM_ATTR const uint32_t jag_6d_cbits[3][4] = {
    {P1_B0_MASK, 0, P1_B0_MASK, 0},
    {0, 0, P1_B0_MASK, 0},
    {0, 0, P1_B0_MASK, 0},
};
static DRAM_ATTR const uint32_t jag_6d_bbits[3][4] = {
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {P1_B1_MASK, 0, 0, 0},
};


void IRAM_ATTR jag_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        default:
        {
            struct jag_map *map = (struct jag_map *)wired_data->output;

            for (uint32_t i = 0; i < 4; i++) {
                map->buttons[i] = 0xFFFDFFFD;
            }
            for (uint32_t i = 0; i < 3; i++) {
                for (uint32_t j = 0; j < 4; j++) {
                    map->buttons_s1[i][j] = 0xFFFDFFFD & ~(NIBBLE_MASK | jag_6d_cbits[i][j] | jag_6d_bbits[i][j]);
                }
            }
            if (config.global_cfg.multitap_cfg == MT_SLOT_1) {
                struct jag_map *map3 = (struct jag_map *)wired_adapter.data[3].output;

                map3->buttons[1] = 0xFFFDFFFD & ~P1_B0_MASK;
                for (uint32_t i = 0; i < 3; i++) {
                    map3->buttons_s1[i][1] &= ~P1_B0_MASK;
                }
            }
            break;
        }
    }
}

void jag_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_PAD_ALT:
                    ctrl_data[i].mask = jag_6d_mask;
                    ctrl_data[i].desc = jag_6d_desc;
                    ctrl_data[i].axes[j].meta = &jag_6d_axes_meta[j];
                    break;
                default:
                    ctrl_data[i].mask = jag_mask;
                    ctrl_data[i].desc = jag_desc;
            }
        }
    }
}

static void jag_ctrl_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct jag_map map_tmp;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                for (uint32_t j = 0; j < 4; j++) {
                    map_tmp.buttons[j] &= ~jag_btns_mask[j][i];
                }
            }
            else {
                for (uint32_t j = 0; j < 4; j++) {
                    map_tmp.buttons[j] |= jag_btns_mask[j][i];
                }
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

static void jag_6d_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct jag_map map_tmp;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                for (uint32_t j = 0; j < 4; j++) {
                    map_tmp.buttons[j] &= ~jag_btns_mask[j][i];
                }
                if (jag_6d_btns_idx[i]) {
                    map_tmp.buttons_s1[(jag_6d_btns_idx[i] >> 4) & 0x3][jag_6d_btns_idx[i] & 0xF] &= ~P1_B1_MASK;
                }
            }
            else {
                for (uint32_t j = 0; j < 4; j++) {
                    map_tmp.buttons[j] |= jag_btns_mask[j][i];
                }
                if (jag_6d_btns_idx[i]) {
                    map_tmp.buttons_s1[(jag_6d_btns_idx[i] >> 4) & 0x3][jag_6d_btns_idx[i] & 0xF] |= P1_B1_MASK;
                }
            }
        }
    }

    for (uint32_t i = 0; i < ADAPTER_MAX_AXES; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & jag_6d_desc[0])) {
            uint8_t tmp = ctrl_data->axes[i].meta->neutral;

            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                tmp = 127;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                tmp = -128;
            }
            else {
                tmp = (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }

            map_tmp.buttons_s1[jag_6d_axes_idx[i].bank_hi][jag_6d_axes_idx[i].row_hi] &= ~NIBBLE_MASK;
            map_tmp.buttons_s1[jag_6d_axes_idx[i].bank_lo][jag_6d_axes_idx[i].row_lo] &= ~NIBBLE_MASK;
            for (uint32_t j = 0, mask_l = 0x01, mask_h = 0x10; j < 4; ++j, mask_l <<= 1, mask_h <<= 1) {
                if (mask_h & tmp) {
                    map_tmp.buttons_s1[jag_6d_axes_idx[i].bank_hi][jag_6d_axes_idx[i].row_hi] |= jag_6d_nibble_mask[j];
                }
                if (mask_l & tmp) {
                    map_tmp.buttons_s1[jag_6d_axes_idx[i].bank_lo][jag_6d_axes_idx[i].row_lo] |= jag_6d_nibble_mask[j];
                }
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

void jag_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_PAD_ALT:
            jag_6d_from_generic(ctrl_data, wired_data);
            break;
        default:
            jag_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
    jag_io_force_update();
}
