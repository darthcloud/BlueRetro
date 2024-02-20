// Copyright 2015-2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <xtensa/coreasm.h>
#include <xtensa/corebits.h>
#include <xtensa/config/system.h>
#include <xtensa_context.h>
#include "freertos/xtensa_rtos.h"
#include "esp_private/panic_reason.h"
#include "sdkconfig.h"
#include "soc/soc.h"
#include "soc/soc_caps.h"
#include "soc/dport_reg.h"
#include "soc/timer_group_reg.h"

/*

Interrupt , a high-priority interrupt, is used for several things:
- Core0 stall

*/

#define L5_INTR_STACK_SIZE  12
#define L5_INTR_A2_OFFSET   0
#define L5_INTR_A3_OFFSET   4
#define L5_INTR_A4_OFFSET   8
    .data
_l5_intr_stack:
    .space      L5_INTR_STACK_SIZE /* This allocates stacks  */

    .section .iram1,"ax"
    .global     xt_highint5
    .type       xt_highint5,@function
    .align      4
xt_highint5:
    /* See if we're here for the dport access interrupt */
    rsr     a0, INTERRUPT
    extui   a0, a0, 31, 1
    bnez    a0, .handle_dport_access_int

    rsr     a0, EXCSAVE_5                   /* restore a0 */
    rfi     5

    .align      4
.handle_dport_access_int:
    /* This section is for dport access register protection */
    /* Allocate exception frame and save minimal context. */
    /* Because the interrupt cause code has protection that only
       allows one cpu to enter in the dport section of the L5
       interrupt at one time, there's no need to have two
       _l5_intr_stack for each cpu */

    /* This int is edge-triggered and needs clearing. */
    //movi    a0, (1<<ETS_DPORT_INUM)
    //wsr     a0, INTCLEAR

    /* Save A2, A3, A4 so we can use those registers */
    movi    a0, _l5_intr_stack
    s32i    a2, a0, L5_INTR_A2_OFFSET
    s32i    a3, a0, L5_INTR_A3_OFFSET
    s32i    a4, a0, L5_INTR_A4_OFFSET

    /* current cpu is 0 */
    movi    a0, DPORT_CPU_INTR_FROM_CPU_2_REG
    movi    a2, 0
    s32i    a2, a0, 0   /* clear intr */
    movi    a0, 0

    rsil    a4, SOC_DPORT_WORKAROUND_DIS_INTERRUPT_LVL /* disable nested iterrupt */
    /* set and wait flag */
    movi    a2, core0_stall_start_var
    addx4   a2, a0, a2
    movi    a3, 1
    s32i    a3, a2, 0
    memw
    movi    a2, core0_stall_end_var
    addx4   a2, a0, a2
.check_dport_access_end:
    l32i    a3, a2, 0
    beqz    a3, .check_dport_access_end

    wsr     a4, PS                                   /* restore iterrupt level */
    /* Done. Restore registers and return. */
    movi    a0, _l5_intr_stack
    l32i    a2, a0, L5_INTR_A2_OFFSET
    l32i    a3, a0, L5_INTR_A3_OFFSET
    l32i    a4, a0, L5_INTR_A4_OFFSET
    rsync                                   /* ensure register restored */

    rsr     a0, EXCSAVE_5                   /* restore a0 */
    rfi     5

/* The linker has no reason to link in this file; all symbols it exports are already defined
   (weakly!) in the default int handler. Define a symbol here so we can use it to have the
   linker inspect this anyway. */

    .global ld_include_core0_stall_highint_hdl
ld_include_core0_stall_highint_hdl:
