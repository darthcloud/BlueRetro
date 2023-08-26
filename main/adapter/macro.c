/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr/types.h"
#include "tools/util.h"
#include "system/manager.h"
#include "adapter/config.h"
#include "macro.h"

typedef void (*config_change_t)(uint32_t index);

#define BIT32(n) (1UL << ((n) & 0x1F))

#define MACRO_BASE (BIT32(BR_COMBO_BASE_1) | BIT32(BR_COMBO_BASE_2) | BIT32(BR_COMBO_BASE_3))
#define SYS_RESET (MACRO_BASE | BIT32(BR_COMBO_SYS_RESET))
#define BT_INQUIRY (MACRO_BASE | BIT32(BR_COMBO_BT_INQUIRY))
#define SYS_POWER_OFF (MACRO_BASE | BIT32(BR_COMBO_SYS_POWER_OFF))
#define FACTORY_RESET (MACRO_BASE | BIT32(BR_COMBO_BASE_4) | BIT32(BR_COMBO_FACTORY_RESET))
#define DEEP_SLEEP (MACRO_BASE | BIT32(BR_COMBO_BASE_4) | BIT32(BR_COMBO_DEEP_SLEEP))
#define WIRED_RST (MACRO_BASE | BIT32(BR_COMBO_WIRED_RST))

struct macro {
    uint32_t macro;
    uint8_t sys_mgr_cmd;
    uint32_t flag_mask;
    config_change_t cfg_func;
};

static void update_cfg_mode(uint32_t index);

static struct macro macros[] = {
    {.macro = SYS_RESET, .sys_mgr_cmd = SYS_MGR_CMD_RST, .flag_mask = BT_WAITING_FOR_RELEASE_MACRO1},
    {.macro = BT_INQUIRY, .sys_mgr_cmd = SYS_MGR_CMD_INQ_TOOGLE, .flag_mask = BT_WAITING_FOR_RELEASE_MACRO2},
    {.macro = SYS_POWER_OFF, .sys_mgr_cmd = SYS_MGR_CMD_PWR_OFF, .flag_mask = BT_WAITING_FOR_RELEASE_MACRO3},
    {.macro = FACTORY_RESET, .sys_mgr_cmd = SYS_MGR_CMD_FACTORY_RST, .flag_mask = BT_WAITING_FOR_RELEASE_MACRO4},
    {.macro = DEEP_SLEEP, .sys_mgr_cmd = SYS_MGR_CMD_DEEP_SLEEP, .flag_mask = BT_WAITING_FOR_RELEASE_MACRO5},
    {.macro = WIRED_RST, .sys_mgr_cmd = SYS_MGR_CMD_WIRED_RST, .flag_mask = BT_WAITING_FOR_RELEASE_MACRO6, .cfg_func = update_cfg_mode},
};

static void update_cfg_mode(uint32_t index) {
    config.out_cfg[index].dev_mode &= 0x01;
    config.out_cfg[index].dev_mode ^= 0x01;
}

static void check_macro(uint32_t index, int32_t value, struct macro *macro, atomic_t *flags) {
    if (value == macro->macro) {
        if (!atomic_test_bit(flags, macro->flag_mask)) {
            atomic_set_bit(flags, macro->flag_mask);
        }
    }
    else if (atomic_test_bit(flags, macro->flag_mask)) {
        atomic_clear_bit(flags, macro->flag_mask);

        printf("# %s: Apply macro %08lX %08lX\n", __FUNCTION__, value, macro->macro);
        if (macro->cfg_func) {
            macro->cfg_func(index);
        }
        sys_mgr_cmd(macro->sys_mgr_cmd);
    }
}

void sys_macro_hdl(struct wired_ctrl *ctrl_data, atomic_t *flags) {
    int32_t value = ctrl_data->btns[3].value;

    for (uint32_t i = 0; i < sizeof(macros)/sizeof(macros[0]); i++) {
        struct macro *macro = &macros[i];

        check_macro(ctrl_data->index, value, macro, flags);
    }
}
