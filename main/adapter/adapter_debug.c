/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/adapter_debug.h"

#ifdef CONFIG_BLUERETRO_ADAPTER_BTNS_DBG
static void adapter_debug_btns(int32_t value) {
        uint32_t b = value;
        printf(" %sDL%s %sDR%s %sDD%s %sDU%s %sBL%s %sBR%s %sBD%s %sBU%s %sMM%s %sMS%s %sMT%s %sMQ%s %sLM%s %sLS%s %sLT%s %sLJ%s %sRM%s %sRS%s %sRT%s %sRJ%s",
            (b & BIT(PAD_LD_LEFT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LD_RIGHT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LD_DOWN)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LD_UP)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RB_LEFT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RB_RIGHT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RB_DOWN)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RB_UP)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_MM)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_MS)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_MT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_MQ)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LM)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LS)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_LJ)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RM)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RS)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RT)) ? GREEN : RESET, RESET,
            (b & BIT(PAD_RJ)) ? GREEN : RESET, RESET);
}
#endif

void adapter_debug_print(struct generic_ctrl *ctrl_input) {
        printf("LX: %s%08lX%s, LY: %s%08lX%s, RX: %s%08lX%s, RY: %s%08lX%s, LT: %s%08lX%s, RT: %s%08lX%s, BTNS: %s%08lX%s, BTNS: %s%08lX%s, BTNS: %s%08lX%s, BTNS: %s%08lX%s",
            BOLD, ctrl_input->axes[0].value, RESET, BOLD, ctrl_input->axes[1].value, RESET, BOLD, ctrl_input->axes[2].value, RESET, BOLD, ctrl_input->axes[3].value, RESET,
            BOLD, ctrl_input->axes[4].value, RESET, BOLD, ctrl_input->axes[5].value, RESET, BOLD, ctrl_input->btns[0].value, RESET, BOLD, ctrl_input->btns[1].value, RESET,
            BOLD, ctrl_input->btns[2].value, RESET, BOLD, ctrl_input->btns[3].value, RESET);
#ifdef CONFIG_BLUERETRO_ADAPTER_BTNS_DBG
        adapter_debug_btns(ctrl_input->btns[0].value);
#endif
        printf("\n");
}
