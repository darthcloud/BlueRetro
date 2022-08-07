/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr/types.h"
#include "tools/util.h"
#include "system/manager.h"
#include "macro.h"

#define MACRO_BASE (BIT(PAD_MM) | BIT(PAD_LM) | BIT(PAD_RM))
#define SYS_RESET (MACRO_BASE | BIT(PAD_RB_LEFT))
#define BT_INQUIRY (MACRO_BASE | BIT(PAD_RB_RIGHT))
#define SYS_POWER_OFF (MACRO_BASE | BIT(PAD_RB_DOWN))
#define FACTORY_RESET (MACRO_BASE | BIT(PAD_LD_UP) | BIT(PAD_RB_UP))
#define DEEP_SLEEP (MACRO_BASE | BIT(PAD_LD_DOWN) | BIT(PAD_RB_UP))

struct macro {
    uint32_t macro;
    void (*action)(void);
    uint32_t flag_mask;
};

static struct macro macros[] = {
    {.macro = SYS_RESET, .action = sys_mgr_reset, .flag_mask = BT_WAITING_FOR_RELEASE_MACRO1},
    {.macro = BT_INQUIRY, .action = sys_mgr_inquiry, .flag_mask = BT_WAITING_FOR_RELEASE_MACRO2},
    {.macro = SYS_POWER_OFF, .action = sys_mgr_power_off, .flag_mask = BT_WAITING_FOR_RELEASE_MACRO3},
    {.macro = FACTORY_RESET, .action = sys_mgr_factory_reset, .flag_mask = BT_WAITING_FOR_RELEASE_MACRO4},
    {.macro = DEEP_SLEEP, .action = sys_mgr_deep_sleep, .flag_mask = BT_WAITING_FOR_RELEASE_MACRO5},
};

static void check_macro(int32_t value, uint32_t map_mask, struct macro *macro, atomic_t *flags) {
    if ((value ^ macro->macro) == 0) {
        if (!atomic_test_bit(flags, macro->flag_mask)) {
            atomic_set_bit(flags, macro->flag_mask);
        }
    }
    else {
        if (atomic_test_bit(flags, macro->flag_mask)) {
            atomic_clear_bit(flags, macro->flag_mask);

            printf("# %s: Apply macro %08lX\n", __FUNCTION__, macro->macro);
            if (macro->action) {
                macro->action();
            }
        }
    }
}

void sys_macro_hdl(struct generic_ctrl *ctrl_data, atomic_t *flags) {
    int32_t value = ctrl_data->btns[0].value;
    uint32_t map_mask = ctrl_data->map_mask[0];

    if (*ctrl_data->desc & BIT(PAD_LM)) {
        int32_t threshold = (int32_t)(((float)50.0/100) * ctrl_data->axes[TRIG_L].meta->abs_max);

        if (ctrl_data->axes[TRIG_L].value > threshold) {
            value |= BIT(PAD_LM);
            map_mask |= BIT(PAD_LM);
        }
    }

    if (*ctrl_data->desc & BIT(PAD_RM)) {
        int32_t threshold = (int32_t)(((float)50.0/100) * ctrl_data->axes[TRIG_R].meta->abs_max);

        if (ctrl_data->axes[TRIG_R].value > threshold) {
            value |= BIT(PAD_RM);
            map_mask |= BIT(PAD_RM);
        }
    }

    for (uint32_t i = 0; i < sizeof(macros)/sizeof(macros[0]); i++) {
        struct macro *macro = &macros[i];

        check_macro(value, map_mask, macro, flags);
    }
}
