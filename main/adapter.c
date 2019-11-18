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

struct generic_ctrl ctrl_input[WIRED_MAX_DEV];
struct generic_ctrl ctrl_output[WIRED_MAX_DEV];
struct bt_adapter bt_adapter;
struct wired_adapter wired_adapter;

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
    to_generic_func[bt_data->dev_type](bt_data, &ctrl_input[bt_data->dev_id]);

    memcpy((void *)&ctrl_output[bt_data->dev_id], (void *)&ctrl_input[bt_data->dev_id], sizeof(ctrl_output[0]));

    if (wired_adapter.system_id != WIRED_NONE) {
        from_generic_func[wired_adapter.system_id](&ctrl_output[bt_data->dev_id], &wired_adapter.data[bt_data->dev_id]);
    }
}
