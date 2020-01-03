#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <xtensa/hal.h>
#include "sdkconfig.h"
#include "../zephyr/types.h"
#include "../util.h"
#include "config.h"
#include "adapter.h"
#include "n64.h"
#include "dc.h"
#include "gc.h"
#include "ps3.h"
#include "wii.h"
#include "ps4.h"
#include "xb1.h"
#include "sw.h"

const uint32_t hat_to_ld_btns[16] = {
    BIT(PAD_LD_UP), BIT(PAD_LD_UP) | BIT(PAD_LD_RIGHT), BIT(PAD_LD_RIGHT), BIT(PAD_LD_DOWN) | BIT(PAD_LD_RIGHT),
    BIT(PAD_LD_DOWN), BIT(PAD_LD_DOWN) | BIT(PAD_LD_LEFT), BIT(PAD_LD_LEFT), BIT(PAD_LD_UP) | BIT(PAD_LD_LEFT),
};

const uint32_t generic_btns_mask[32] = {
    BIT(PAD_LX_LEFT), BIT(PAD_LX_RIGHT), BIT(PAD_LY_DOWN), BIT(PAD_LY_UP),
    BIT(PAD_RX_LEFT), BIT(PAD_RX_RIGHT), BIT(PAD_RY_DOWN), BIT(PAD_RY_UP),
    BIT(PAD_LD_LEFT), BIT(PAD_LD_RIGHT), BIT(PAD_LD_DOWN), BIT(PAD_LD_UP),
    BIT(PAD_RD_LEFT), BIT(PAD_RD_RIGHT), BIT(PAD_RD_DOWN), BIT(PAD_RD_UP),
    BIT(PAD_RB_LEFT), BIT(PAD_RB_RIGHT), BIT(PAD_RB_DOWN), BIT(PAD_RB_UP),
    BIT(PAD_MM), BIT(PAD_MS), BIT(PAD_MT), BIT(PAD_MQ),
    BIT(PAD_LM), BIT(PAD_LS), BIT(PAD_LT), BIT(PAD_LJ),
    BIT(PAD_RM), BIT(PAD_RS), BIT(PAD_RT), BIT(PAD_RJ),
};

static to_generic_t to_generic_func[BT_MAX] = {
    NULL,
    NULL,
    NULL,
    ps3_to_generic,
    NULL,
    NULL,
    NULL,
    wiiu_to_generic,
    ps4_to_generic,
    xb1_to_generic,
    sw_to_generic,
};

static from_generic_t from_generic_func[WIRED_MAX] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    n64_from_generic,
    dc_from_generic,
    NULL,
    gc_from_generic,
    NULL,
};

static fb_to_generic_t fb_to_generic_func[WIRED_MAX] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static fb_from_generic_t fb_from_generic_func[BT_MAX] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static meta_init_t meta_init_func[WIRED_MAX] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    n64_meta_init,
    dc_meta_init,
    NULL,
    gc_meta_init,
    NULL,
};

static buffer_init_t buffer_init_func[WIRED_MAX] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    n64_init_buffer,
    dc_init_buffer,
    NULL,
    gc_init_buffer,
    NULL,
};

struct generic_ctrl ctrl_input;
struct generic_ctrl ctrl_output[WIRED_MAX_DEV];
struct generic_fb fb_input;
struct bt_adapter bt_adapter = {0};
struct wired_adapter wired_adapter = {0};

static uint32_t btn_id_to_btn_idx(uint8_t btn_id) {
    if (btn_id < 32) {
        return 0;
    }
    else if (btn_id >= 32 && btn_id < 64) {
        return 1;
    }
    else if (btn_id >= 64 && btn_id < 96) {
        return 2;
    }
    else {
        return 3;
    }
}

static uint32_t adapter_map_from_axis(struct map_cfg * map_cfg) {
    uint32_t out_mask = BIT(map_cfg->dst_id);
    struct generic_ctrl *out = &ctrl_output[map_cfg->dst_id];
    uint8_t src = map_cfg->src_btn;
    uint8_t dst = map_cfg->dst_btn;
    uint32_t dst_mask = BIT(dst & 0x1F);
    uint32_t dst_btn_idx = btn_id_to_btn_idx(dst);
    uint32_t src_axis_idx = btn_id_to_axis(src);
    uint32_t dst_axis_idx = btn_id_to_axis(dst);

    /* Check if mapping dst exist in output */
    if (dst_mask & out->mask[dst_btn_idx]) {
        int32_t abs_src_value = abs(ctrl_input.axes[src_axis_idx].value);
        int32_t src_sign = btn_sign(ctrl_input.axes[src_axis_idx].meta->polarity, src);
        int32_t sign_check = src_sign * ctrl_input.axes[src_axis_idx].value;

        /* Check if the srv value sign match the src mapping sign */
        if (sign_check >= 0) {
            /* Check if dst is an axis */
            if (dst_mask & out->desc[dst_btn_idx]) {
                /* Dst is an axis */
                int32_t deadzone = (int32_t)(((float)map_cfg->perc_deadzone/10000) * ctrl_input.axes[src_axis_idx].meta->abs_max) + ctrl_input.axes[src_axis_idx].meta->deadzone;
                /* Check if axis over deadzone */
                if (abs_src_value > deadzone) {
                    int32_t value = abs_src_value - deadzone;
                    int32_t dst_sign = btn_sign(out->axes[dst_axis_idx].meta->polarity, dst);
                    float scale = ((float)out->axes[dst_axis_idx].meta->abs_max / (ctrl_input.axes[src_axis_idx].meta->abs_max - deadzone)) * (((float)map_cfg->perc_max)/100);
                    float fvalue = dst_sign * value * scale;
                    value = (int32_t)fvalue;

                    if (abs(value) > abs(out->axes[dst_axis_idx].value)) {
                        out->axes[dst_axis_idx].value = value;
                    }
                }
            }
            else {
                /* Dst is a button */
                int32_t threshold = (int32_t)(((float)map_cfg->perc_threshold/100) * ctrl_input.axes[src_axis_idx].meta->abs_max);
                /* Check if axis over threshold */
                if (abs_src_value > threshold) {
                    out->btns[dst_btn_idx].value |= dst_mask;
                }
            }
        }
        /* Flag this dst for update */
        out->map_mask[dst_btn_idx] |= dst_mask;
    }

    return out_mask;
}

static uint32_t adapter_map_from_btn(struct map_cfg * map_cfg, uint32_t src_mask, uint32_t src_btn_idx) {
    uint32_t out_mask = BIT(map_cfg->dst_id);
    struct generic_ctrl *out = &ctrl_output[map_cfg->dst_id];
    uint8_t dst = map_cfg->dst_btn;
    uint32_t dst_mask = BIT(dst & 0x1F);
    uint32_t dst_btn_idx = btn_id_to_btn_idx(dst);

    /* Check if mapping dst exist in output */
    if (dst_mask & out->mask[dst_btn_idx]) {
        /* Check if button pressed */
        if (ctrl_input.btns[src_btn_idx].value & src_mask) {
            /* Check if dst is an axis */
            if (dst_mask & out->desc[dst_btn_idx]) {
                /* Dst is an axis */
                uint32_t axis_id = btn_id_to_axis(dst);
                float fvalue = out->axes[axis_id].meta->abs_max
                            * btn_sign(out->axes[axis_id].meta->polarity, dst)
                            * (((float)map_cfg->perc_max)/100);
                int32_t value = (int32_t)fvalue;

                if (abs(value) > abs(out->axes[axis_id].value)) {
                    out->axes[axis_id].value = value;
                }
            }
            else {
                /* Dst is a button */
                out->btns[dst_btn_idx].value |= dst_mask;
            }
        }
        /* Flag this dst for update */
        out->map_mask[dst_btn_idx] |= dst_mask;
    }

    return out_mask;
}

static uint32_t adapter_mapping(struct in_cfg * in_cfg) {
    uint32_t out_mask = 0;

    for (uint32_t i = 0; i < in_cfg->map_size; i++) {
        uint8_t src = in_cfg->map_cfg[i].src_btn;
        uint32_t src_mask = BIT(src & 0x1F);
        uint32_t src_btn_idx = btn_id_to_btn_idx(src);

        /* Check if mapping src exist in input */
        if (src_mask & ctrl_input.mask[src_btn_idx]) {
            /* Check if src is an axis */
            if (src_mask & ctrl_input.desc[src_btn_idx]) {
                /* Src is an axis */
                out_mask |= adapter_map_from_axis(&in_cfg->map_cfg[i]);
            }
            else {
                /* Src is a button */
                out_mask |= adapter_map_from_btn(&in_cfg->map_cfg[i], src_mask, src_btn_idx);
            }
        }
    }
    return out_mask;
}

uint8_t btn_id_to_axis(uint8_t btn_id) {
    switch (btn_id) {
        case PAD_LX_LEFT:
        case PAD_LX_RIGHT:
            return AXIS_LX;
        case PAD_LY_DOWN:
        case PAD_LY_UP:
            return AXIS_LY;
        case PAD_RX_LEFT:
        case PAD_RX_RIGHT:
            return AXIS_RX;
        case PAD_RY_DOWN:
        case PAD_RY_UP:
            return AXIS_RY;
        case PAD_LM:
            return TRIG_L;
        case PAD_RM:
            return TRIG_R;
    }
    return AXIS_NONE;
}

uint32_t axis_to_btn_mask(uint8_t axis) {
    switch (axis) {
        case AXIS_LX:
            return BIT(PAD_LX_LEFT) | BIT(PAD_LX_RIGHT);
        case AXIS_LY:
            return BIT(PAD_LY_DOWN) | BIT(PAD_LY_UP);
        case AXIS_RX:
            return BIT(PAD_RX_LEFT) | BIT(PAD_RX_RIGHT);
        case AXIS_RY:
            return BIT(PAD_RY_DOWN) | BIT(PAD_RY_UP);
        case TRIG_L:
            return BIT(PAD_LM);
        case TRIG_R:
            return BIT(PAD_RM);
    }
    return 0x00000000;
}

int8_t btn_sign(uint32_t polarity, uint8_t btn_id) {
    switch (btn_id) {
        case PAD_LX_RIGHT:
        case PAD_LY_UP:
        case PAD_RX_RIGHT:
        case PAD_RY_UP:
        case PAD_LM:
        case PAD_RM:
            return polarity ? -1 : 1;
        case PAD_LX_LEFT:
        case PAD_LY_DOWN:
        case PAD_RY_DOWN:
        case PAD_RX_LEFT:
            return polarity ? 1 : -1;
    }
    return 1;
}

void adapter_init_buffer(uint8_t wired_id) {
    if (wired_adapter.system_id != WIRED_NONE && buffer_init_func[wired_adapter.system_id]) {
        buffer_init_func[wired_adapter.system_id](&wired_adapter.data[wired_id]);
    }
}

void adapter_bridge(struct bt_data *bt_data) {
    uint32_t out_mask = 0;
    //uint32_t end, start = xthal_get_ccount();
    //static uint32_t last = 0;
    //uint32_t cur = xthal_get_ccount();
#if 1
    if (bt_data->dev_id != BT_NONE && to_generic_func[bt_data->dev_type]) {
        to_generic_func[bt_data->dev_type](bt_data, &ctrl_input);

        if (wired_adapter.system_id != WIRED_NONE && from_generic_func[wired_adapter.system_id]) {
            meta_init_func[wired_adapter.system_id](ctrl_output);

            out_mask = adapter_mapping(&config.in_cfg[bt_data->dev_id]);

            for (uint32_t i = 0; out_mask; i++, out_mask >>= 1) {
                from_generic_func[wired_adapter.system_id](&ctrl_output[i], &wired_adapter.data[i]);
            }
        }
    }
#endif
    //end = xthal_get_ccount();
    //printf("%dus\n", (end - start)/CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ);
    //printf("%dus\n", (cur - last)/CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ);
    //last = cur;
}

void IRAM_ATTR adapter_bridge_fb(struct wired_data *wired_data) {
    if (wired_adapter.system_id != WIRED_NONE && fb_to_generic_func[wired_adapter.system_id]) {
        fb_to_generic_func[wired_adapter.system_id](wired_data, &fb_input);
    }
}

void IRAM_ATTR adapter_q_fb(uint8_t *data, uint32_t len) {
    UBaseType_t ret;
    ret = xRingbufferSendFromISR(wired_adapter.input_q_hdl, data, len, NULL);
    if (ret != pdTRUE) {
        ets_printf("# %s input_q full!\n", __FUNCTION__);
    }
}

void adapter_init(void) {
    wired_adapter.system_id = WIRED_NONE;

    wired_adapter.input_q_hdl = xRingbufferCreate(64, RINGBUF_TYPE_NOSPLIT);
    if (wired_adapter.input_q_hdl == NULL) {
        printf("# %s: Failed to create ring buffer\n", __FUNCTION__);
    }
}
