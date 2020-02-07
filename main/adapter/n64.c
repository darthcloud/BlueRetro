#include <string.h>
#include "../zephyr/types.h"
#include "../util.h"
#include "n64.h"

enum {
    N64_LD_RIGHT,
    N64_LD_LEFT,
    N64_LD_DOWN,
    N64_LD_UP,
    N64_START,
    N64_Z,
    N64_B,
    N64_A,
    N64_C_RIGHT,
    N64_C_LEFT,
    N64_C_DOWN,
    N64_C_UP,
    N64_R,
    N64_L,
};

const uint8_t n64_axes_idx[2] =
{
/*  AXIS_LX, AXIS_LY  */
    0,       1
};

const struct ctrl_meta n64_btns_meta =
{
    .polarity = 0,
};

const struct ctrl_meta n64_axes_meta[2] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x54},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x54},
};

struct n64_map {
    uint16_t buttons;
    uint8_t axes[2];
} __packed;

const uint32_t n64_mask[4] = {0x331F0FFF, 0x00000000, 0x00000000, 0x00000000};
const uint32_t n64_desc[4] = {0x0000000F, 0x00000000, 0x00000000, 0x00000000};
const uint32_t n64_btns_mask[32] = {
    0, 0, 0, 0,
    BIT(N64_C_LEFT), BIT(N64_C_RIGHT), BIT(N64_C_DOWN), BIT(N64_C_UP),
    BIT(N64_LD_LEFT), BIT(N64_LD_RIGHT), BIT(N64_LD_DOWN), BIT(N64_LD_UP),
    0, 0, 0, 0,
    BIT(N64_B), BIT(N64_C_DOWN), BIT(N64_A), BIT(N64_C_LEFT),
    BIT(N64_START), 0, 0, 0,
    BIT(N64_Z), BIT(N64_L), 0, 0,
    BIT(N64_Z), BIT(N64_R), 0, 0,
};

const uint32_t n64_mouse_mask[4] = {0x110000F0, 0x00000000, 0x00000000, 0x00000000};
const uint32_t n64_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
const uint32_t n64_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(N64_B), 0, 0, 0,
    BIT(N64_A), 0, 0, 0,
};

void n64_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    struct n64_map *map = (struct n64_map *)wired_data->output;

    map->buttons = 0x0000;
    for (uint32_t i = 0; i < ARRAY_SIZE(n64_axes_meta); i++) {
        map->axes[n64_axes_idx[i]] = n64_axes_meta[i].neutral;
    }
}

void n64_meta_init(int32_t dev_mode, struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*4);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ARRAY_SIZE(n64_axes_meta); j++) {
            switch (dev_mode) {
                case DEV_MOUSE:
                    ctrl_data[i].mask = n64_mouse_mask;
                    ctrl_data[i].desc = n64_mouse_desc;
                    ctrl_data[i].axes[j + 2].meta = &n64_axes_meta[j];
                    break;
                case DEV_PAD:
                default:
                    ctrl_data[i].mask = n64_mask;
                    ctrl_data[i].desc = n64_desc;
                    ctrl_data[i].axes[j].meta = &n64_axes_meta[j];
                    break;
            }
        }
    }
}

static void n64_ctrl_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct n64_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & generic_btns_mask[i]) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= n64_btns_mask[i];
                map_mask &= ~n64_btns_mask[i];
            }
            else if (map_mask & n64_btns_mask[i]) {
                map_tmp.buttons &= ~n64_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 0; i < ARRAY_SIZE(n64_axes_meta); i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & n64_desc[0])) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                map_tmp.axes[n64_axes_idx[i]] = 127;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                map_tmp.axes[n64_axes_idx[i]] = -128;
            }
            else {
                map_tmp.axes[n64_axes_idx[i]] = (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

static void n64_mouse_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct n64_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    *(int32_t *)&map_tmp = atomic_get((int32_t *)&wired_data->output);

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & generic_btns_mask[i]) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= n64_mouse_btns_mask[i];
                map_mask &= ~n64_mouse_btns_mask[i];
            }
            else if (map_mask & n64_mouse_btns_mask[i]) {
                map_tmp.buttons &= ~n64_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 0; i < ARRAY_SIZE(n64_axes_meta); i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i + 2) & n64_mouse_desc[0])) {
            if (ctrl_data->axes[i + 2].value > ctrl_data->axes[i + 2].meta->size_max) {
                map_tmp.axes[n64_axes_idx[i]] = 127;
            }
            else if (ctrl_data->axes[i + 2].value < ctrl_data->axes[i + 2].meta->size_min) {
                map_tmp.axes[n64_axes_idx[i]] = -128;
            }
            else {
                map_tmp.axes[n64_axes_idx[i]] += (uint8_t)(ctrl_data->axes[i + 2].value + ctrl_data->axes[i + 2].meta->neutral);
            }
        }
    }

    atomic_set((int32_t *)&wired_data->output, *(int32_t *)&map_tmp);
}

void n64_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_MOUSE:
            n64_mouse_from_generic(ctrl_data, wired_data);
            break;
        case DEV_PAD:
        default:
            n64_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
}

void n64_fb_to_generic(int32_t dev_mode, uint8_t *raw_fb_data, uint32_t raw_fb_len, struct generic_fb *fb_data) {
    fb_data->wired_id = raw_fb_data[0];
    fb_data->state = raw_fb_data[1];
    fb_data->cycles = 0;
    fb_data->start = 0;
}
