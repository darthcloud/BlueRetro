#include <stdio.h>
#include <string.h>
#include "zephyr/types.h"
#include "util.h"
#include "config.h"
#include "adapter.h"
#include "wii.h"
#include "dc.h"

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
    NULL,
    NULL,
    NULL,
    NULL,
    wiiu_to_generic,
    NULL,
    NULL,
    NULL,
};

static from_generic_t from_generic_func[WIRED_MAX] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    dc_from_generic,
    NULL,
    NULL,
    NULL,
};

struct generic_ctrl ctrl_input;
struct generic_ctrl ctrl_output[WIRED_MAX_DEV];
struct bt_adapter bt_adapter;
struct wired_adapter wired_adapter;

static uint32_t adapter_map_from_axis(struct in_cfg * in_cfg) {
    uint32_t out_mask = 0;
    return out_mask;
}

static uint32_t adapter_map_from_btn(struct in_cfg * in_cfg, uint32_t btn_idx) {
    uint32_t out_mask = 0;
    return out_mask;
}

static uint32_t adapter_mapping(struct in_cfg * in_cfg) {
    uint32_t out_mask = 0;

    for (uint32_t i = 0; i < in_cfg->map_size; i++) {
        uint8_t source = in_cfg->map_cfg[i].src_btn;

        if (ctrl_input.mask[0] && source < 32 && BIT(source & 0x1F) & ctrl_input.mask[0]) {
            if (BIT(source & 0x1F) & ctrl_input.desc[0]) {
                /* Source is Axis */
                out_mask |= adapter_map_from_axis(in_cfg);
            }
            else {
                /* Source is Button */
                out_mask |= adapter_map_from_btn(in_cfg, 0);
            }
        }
        else if (ctrl_input.mask[1] && source >= 32 && source < 64 && BIT(source & 0x1F) & ctrl_input.mask[1]) {
            out_mask |= adapter_map_from_btn(in_cfg, 1);
        }
        else if (ctrl_input.mask[2] && source >= 64 && source < 96 && BIT(source & 0x1F) & ctrl_input.mask[2]) {
            out_mask |= adapter_map_from_btn(in_cfg, 2);
        }
        else if (ctrl_input.mask[3] && source >= 96 && BIT(source & 0x1F) & ctrl_input.mask[3]) {
            out_mask |= adapter_map_from_btn(in_cfg, 3);
        }
    }
    return out_mask;
}

void adapter_bridge(struct bt_data *bt_data) {
#if 0
    uint8_t data[] = {0x00, 0xFF, 0x00, 0xFF};
    struct ctrl_desc ctrl_test;
    ctrl_test.data[BTNS0].type = U8_TYPE;
    ctrl_test.data[BTNS0].pval.u8 = &data[1];
    ctrl_test.data[BTNS1].type = U16_TYPE;
    ctrl_test.data[BTNS1].pval.u16 = (uint16_t *)&data[2];
    ctrl_test.data[LX_AXIS].type = U32_TYPE;
    ctrl_test.data[LX_AXIS].pval.u32 = (uint32_t *)&data[0];
    printf("btns0: %X btns1: %X lx_axis: %X\n", CTRL_DATA(ctrl_test.data[BTNS0]), CTRL_DATA(ctrl_test.data[BTNS1]), CTRL_DATA(ctrl_test.data[LX_AXIS]));
    wiiu_init_desc(bt_data);
#endif
    uint32_t out_mask = 0;

    if (bt_data->dev_id != BT_NONE && to_generic_func[bt_data->dev_type]) {
        to_generic_func[bt_data->dev_type](bt_data, &ctrl_input);

        /* Need to init output meta here */

        out_mask = adapter_mapping(&config.in_cfg[bt_data->dev_type]);

        if (wired_adapter.system_id != WIRED_NONE && from_generic_func[wired_adapter.system_id]) {
            for (uint32_t i = 0; out_mask; i++, out_mask >>= 1) {
                /* this need to reset only what was mapped so adapter_mapping need to provide a btn mask */
                from_generic_func[wired_adapter.system_id](&ctrl_output[i], &wired_adapter.data[i]);
            }
        }
    }
}
