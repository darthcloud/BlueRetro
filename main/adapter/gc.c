#include <string.h>
#include "../zephyr/types.h"
#include "../util.h"
#include "gc.h"

enum {
    GC_A,
    GC_B,
    GC_X,
    GC_Y,
    GC_START,
    GC_LD_LEFT = 8,
    GC_LD_RIGHT,
    GC_LD_DOWN,
    GC_LD_UP,
    GC_Z,
    GC_R,
    GC_L,
};

const uint8_t gc_axes_idx[6] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       2,       3,       4,      5
};

const struct ctrl_meta gc_btns_meta =
{
    .polarity = 0,
};

const struct ctrl_meta gc_axes_meta[6] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x64},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x64},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x5C},
    {.size_min = -128, .size_max = 127, .neutral = 0x80, .abs_max = 0x5C},
    {.size_min = 0, .size_max = 255, .neutral = 0x20, .abs_max = 0xD0},
    {.size_min = 0, .size_max = 255, .neutral = 0x20, .abs_max = 0xD0},
};

struct gc_map {
    uint16_t buttons;
    uint8_t axes[6];
} __packed;

const uint32_t gc_mask[4] = {0x771F0FFF, 0x00000000, 0x00000000, 0x00000000};
const uint32_t gc_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};

const uint32_t gc_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(GC_LD_LEFT), BIT(GC_LD_RIGHT), BIT(GC_LD_DOWN), BIT(GC_LD_UP),
    0, 0, 0, 0,
    BIT(GC_B), BIT(GC_X), BIT(GC_A), BIT(GC_Y),
    BIT(GC_START), 0, 0, 0,
    0, BIT(GC_Z), BIT(GC_L), 0,
    0, BIT(GC_Z), BIT(GC_R), 0,
};

void gc_init_buffer(struct wired_data *wired_data) {
    struct gc_map *map = (struct gc_map *)wired_data->output;

    map->buttons = 0x8020;
    for (uint32_t i = 0; i < ARRAY_SIZE(gc_axes_meta); i++) {
        map->axes[gc_axes_idx[i]] = gc_axes_meta[i].neutral;
    }
}

void gc_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*4);

    for (uint32_t i = 0; i < WIRED_MAX; i++) {
        for (uint32_t j = 0; j < ARRAY_SIZE(gc_axes_meta); j++) {
            ctrl_data[i].mask = gc_mask;
            ctrl_data[i].desc = gc_desc;
            ctrl_data[i].axes[j].meta = &gc_axes_meta[j];
        }
    }
}

void gc_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct gc_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & generic_btns_mask[i]) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= gc_btns_mask[i];
                map_mask &= ~gc_btns_mask[i];
            }
            else if (map_mask & gc_btns_mask[i]) {
                map_tmp.buttons &= ~gc_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 0; i < ARRAY_SIZE(gc_axes_meta); i++) {
        if (ctrl_data->map_mask[0] & axis_to_btn_mask(i)) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                map_tmp.axes[gc_axes_idx[i]] = 127;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                map_tmp.axes[gc_axes_idx[i]] = -128;
            }
            else {
                map_tmp.axes[gc_axes_idx[i]] = (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}
