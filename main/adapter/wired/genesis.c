/*
 * Copyright (c) 2020-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "adapter/config.h"
#include "adapter/wired/wired.h"
#include "system/manager.h"
#include "zephyr/types.h"
#include "tools/util.h"
#include "genesis.h"
#include "driver/gpio.h"

#define P1_TH_PIN 35
#define P1_TR_PIN 27
#define P1_TL_PIN 26
#define P1_R_PIN 23
#define P1_L_PIN 18
#define P1_D_PIN 5
#define P1_U_PIN 3

#define P2_TH_PIN 36
#define P2_TR_PIN 16
#define P2_TL_PIN 33
#define P2_R_PIN 25
#define P2_L_PIN 22
#define P2_D_PIN 21
#define P2_U_PIN 19

#define P1_A P1_TL_PIN
#define P1_B P1_TL_PIN
#define P1_C P1_TR_PIN
#define P1_START P1_TR_PIN
#define P1_LD_UP P1_U_PIN
#define P1_LD_DOWN P1_D_PIN
#define P1_LD_LEFT P1_L_PIN
#define P1_LD_RIGHT P1_R_PIN
#define P1_Z P1_U_PIN
#define P1_Y P1_D_PIN
#define P1_X P1_L_PIN
#define P1_MODE P1_R_PIN

#define P2_A P2_TL_PIN
#define P2_B P2_TL_PIN
#define P2_C P2_TR_PIN
#define P2_START P2_TR_PIN
#define P2_LD_UP P2_U_PIN
#define P2_LD_DOWN P2_D_PIN
#define P2_LD_LEFT P2_L_PIN
#define P2_LD_RIGHT P2_R_PIN
#define P2_Z P2_U_PIN
#define P2_Y P2_D_PIN
#define P2_X P2_L_PIN
#define P2_MODE P2_R_PIN

#define P1_MOUSE_ID0_HI 0xFF79FFD5
#define P1_MOUSE_ID0_LO 0xFFF9FFFD
#define P2_MOUSE_ID0_HI 0xFD95FFFD
#define P2_MOUSE_ID0_LO 0xFFBDFFFD

enum {
    GENESIS_B = 0,
    GENESIS_C,
    GENESIS_A,
    GENESIS_START,
    GENESIS_LD_UP,
    GENESIS_LD_DOWN,
    GENESIS_LD_LEFT,
    GENESIS_LD_RIGHT,
    GENESIS_Z = 12,
    GENESIS_Y,
    GENESIS_X,
    GENESIS_MODE,
};

static DRAM_ATTR const uint8_t sega_mouse_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       0,       1,       0,     1
};

static DRAM_ATTR const struct ctrl_meta sega_mouse_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -256, .size_max = 255, .neutral = 0x00, .abs_max = 255, .abs_min = 256},
    {.size_min = -256, .size_max = 255, .neutral = 0x00, .abs_max = 255, .abs_min = 256},
    {.size_min = -256, .size_max = 255, .neutral = 0x00, .abs_max = 255, .abs_min = 256},
    {.size_min = -256, .size_max = 255, .neutral = 0x00, .abs_max = 255, .abs_min = 256},
    {.size_min = -256, .size_max = 255, .neutral = 0x00, .abs_max = 255, .abs_min = 256},
    {.size_min = -256, .size_max = 255, .neutral = 0x00, .abs_max = 255, .abs_min = 256},
};

struct sega_mouse_map {
    uint32_t buttons[3];
    uint32_t buttons_high[3];
    union {
        uint16_t twh_buttons;
        struct {
            uint8_t flags;
            uint8_t align[1];
        };
    };
    uint8_t relative[2];
    int32_t raw_axes[2];
} __packed;

static const uint32_t sega_mouse_mask[4] = {0x190100F0, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t sega_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t sega_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(GENESIS_START), 0, 0, 0,
    0, 0, 0, 0,
    BIT(GENESIS_C), 0, 0, BIT(GENESIS_A),
    BIT(GENESIS_B), 0, 0, 0,
};

struct genesis_map {
    uint32_t buttons[3];
    uint32_t buttons_high[3];
    uint16_t twh_buttons;
} __packed;

static const uint32_t genesis_mask[4] = {0x337F0F00, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t genesis_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t genesis_btns_mask[2][3][32] = {
    {
        /* TH HIGH */
        {
            0, 0, 0, 0,
            0, 0, 0, 0,
            BIT(P1_LD_LEFT), BIT(P1_LD_RIGHT), BIT(P1_LD_DOWN), BIT(P1_LD_UP),
            0, 0, 0, 0,
            0, BIT(P1_C), BIT(P1_B), 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        },
        /* TH LOW */
        {
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, BIT(P1_LD_DOWN), BIT(P1_LD_UP),
            0, 0, 0, 0,
            BIT(P1_A), 0, 0, 0,
            BIT(P1_START), 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        },
        /* 6BTNS */
        {
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, BIT(P1_Y),
            BIT(P1_MODE), 0, 0, 0,
            BIT(P1_X), BIT(P1_X), 0, 0,
            BIT(P1_Z), BIT(P1_Z), 0, 0,
        },
    },
    {
        /* TH HIGH */
        {
            0, 0, 0, 0,
            0, 0, 0, 0,
            BIT(P2_LD_LEFT), BIT(P2_LD_RIGHT), BIT(P2_LD_DOWN), BIT(P2_LD_UP),
            0, 0, 0, 0,
            0, BIT(P2_C), BIT(P2_B - 32) | 0xF0000000, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        },
        {
        /* TH LOW */
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, BIT(P2_LD_DOWN), BIT(P2_LD_UP),
            0, 0, 0, 0,
            BIT(P2_A - 32) | 0xF0000000, 0, 0, 0,
            BIT(P2_START), 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        },
        /* 6BTNS */
        {
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, BIT(P2_Y),
            BIT(P2_MODE), 0, 0, 0,
            BIT(P2_X), BIT(P2_X), 0, 0,
            BIT(P2_Z), BIT(P2_Z), 0, 0,
        },
    },
};

static DRAM_ATTR const uint32_t genesis_twh_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(GENESIS_LD_LEFT), BIT(GENESIS_LD_RIGHT), BIT(GENESIS_LD_DOWN), BIT(GENESIS_LD_UP),
    0, 0, 0, 0,
    BIT(GENESIS_A), BIT(GENESIS_C), BIT(GENESIS_B), BIT(GENESIS_Y),
    BIT(GENESIS_START), BIT(GENESIS_MODE), 0, 0,
    BIT(GENESIS_X), BIT(GENESIS_X), 0, 0,
    BIT(GENESIS_Z), BIT(GENESIS_Z), 0, 0,
};

static void genesis_ctrl_special_action(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    /* Output config mode toggle GamePad/GamePadAlt */
    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_MT]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_MT]) {
            if (!atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_set_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);
            }
        }
        else {
            if (atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_clear_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);

                config.out_cfg[ctrl_data->index].dev_mode &= 0x01;
                config.out_cfg[ctrl_data->index].dev_mode ^= 0x01;
                sys_mgr_cmd(SYS_MGR_CMD_WIRED_RST);
            }
        }
    }
}

static void sega_mouse_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct sega_mouse_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 28);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.twh_buttons |= sega_mouse_btns_mask[i];
            }
            else {
                map_tmp.twh_buttons &= ~sega_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 2; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & sega_mouse_desc[0])) {
            if (ctrl_data->axes[i].relative) {
                map_tmp.relative[sega_mouse_axes_idx[i]] = 1;
                atomic_add(&raw_axes[sega_mouse_axes_idx[i]], ctrl_data->axes[i].value);
            }
            else {
                map_tmp.relative[sega_mouse_axes_idx[i]] = 0;
                raw_axes[sega_mouse_axes_idx[i]] = ctrl_data->axes[i].value;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 8);
}

static void genesis_std_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct genesis_map map_tmp;
    uint32_t map_mask[3];
    uint32_t map_mask_high[3];

    memset(map_mask, 0xFF, sizeof(map_mask));
    memset(map_mask_high, 0xFF, sizeof(map_mask_high));
    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                for (uint32_t j = 0; j < 3; j++) {
                    if ((genesis_btns_mask[ctrl_data->index][j][i] & 0xF0000000) == 0xF0000000) {
                        map_tmp.buttons_high[j] &= ~(genesis_btns_mask[ctrl_data->index][j][i] & 0x000000FF);
                        map_mask_high[j] &= ~(genesis_btns_mask[ctrl_data->index][j][i] & 0x000000FF);
                    }
                    else {
                        map_tmp.buttons[j] &= ~genesis_btns_mask[ctrl_data->index][j][i];
                        map_mask[j] &= ~genesis_btns_mask[ctrl_data->index][j][i];
                    }
                }
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else {
                for (uint32_t j = 0; j < 3; j++) {
                    if ((genesis_btns_mask[ctrl_data->index][j][i] & 0xF0000000) == 0xF0000000) {
                        if (map_mask_high[j] & genesis_btns_mask[ctrl_data->index][j][i] & 0x000000FF) {
                            map_tmp.buttons_high[j] |= genesis_btns_mask[ctrl_data->index][j][i] & 0x000000FF;
                        }
                    }
                    else {
                        if (map_mask[j] & genesis_btns_mask[ctrl_data->index][j][i]) {
                            map_tmp.buttons[j] |= genesis_btns_mask[ctrl_data->index][j][i];
                        }
                    }
                }
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    genesis_ctrl_special_action(ctrl_data, wired_data);

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
    printf("{\"log_type\": \"wired_output\", \"btns\": [%ld, %ld, %ld, %ld, %ld, %ld]}\n",
        map_tmp.buttons[0], map_tmp.buttons[1], map_tmp.buttons[2],
        map_tmp.buttons_high[0], map_tmp.buttons_high[1], map_tmp.buttons_high[2]);
#endif
}

static void genesis_twh_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct genesis_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.twh_buttons &= ~genesis_twh_btns_mask[i];
                map_mask &= ~genesis_twh_btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else if (map_mask & genesis_twh_btns_mask[i]) {
                map_tmp.twh_buttons |=  genesis_twh_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    genesis_ctrl_special_action(ctrl_data, wired_data);

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

static void genesis_ea_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct genesis_map map_tmp;
    uint32_t map_mask[3];

    memset(map_mask, 0xFF, sizeof(map_mask));
    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                for (uint32_t j = 0; j < 3; j++) {
                    map_tmp.buttons[j] &= ~genesis_btns_mask[0][j][i];
                    map_mask[j] &= ~genesis_btns_mask[0][j][i];
                }
            }
            else {
                for (uint32_t j = 0; j < 3; j++) {
                    if (map_mask[j] & genesis_btns_mask[0][j][i]) {
                        map_tmp.buttons[j] |= genesis_btns_mask[0][j][i];
                    }
                }
            }
        }
    }

    genesis_ctrl_special_action(ctrl_data, wired_data);

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

void IRAM_ATTR genesis_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    /* Hackish but wtv */
    struct genesis_map *map1 = (struct genesis_map *)wired_adapter.data[0].output;
    struct genesis_map *map2 = (struct genesis_map *)wired_adapter.data[1].output;
    struct genesis_map *map1_mask = (struct genesis_map *)wired_adapter.data[0].output_mask;
    struct genesis_map *map2_mask = (struct genesis_map *)wired_adapter.data[1].output_mask;

    if (config.global_cfg.multitap_cfg == MT_SLOT_1 || config.global_cfg.multitap_cfg == MT_DUAL) {
        map1->buttons[0] = 0xFF79FFFD;
        map1->buttons[1] = 0xFFFDFFFD;
        map1->buttons[2] = 0xFFFDFFFD;
    }
    else if (config.global_cfg.multitap_cfg == MT_ALT) {
        map1->buttons[0] = 0xFFFDFFFF;
        map1->buttons[1] = 0xFF79FFFF;
        map1->buttons[2] = 0xF379FFD7;
    }
    else if (config.out_cfg[0].dev_mode == DEV_MOUSE) {
        map1->buttons[0] = P1_MOUSE_ID0_HI;
        map1->buttons[1] = P1_MOUSE_ID0_LO;
        map1->buttons[2] = 0xFFFDFFFD;
    }
    else {
        map1->buttons[0] = 0xFFFDFFFD;
        map1->buttons[1] = 0xFF79FFFD;
        map1->buttons[2] = 0xFFFDFFFD;
    }
    memset(map1_mask->buttons, 0, sizeof(map1_mask->buttons));

    if (config.global_cfg.multitap_cfg == MT_SLOT_2 || config.global_cfg.multitap_cfg == MT_DUAL) {
        map2->buttons[0] = 0xFDBDFFFD;
        map2->buttons[1] = 0xFFFDFFFD;
        map2->buttons[2] = 0xFFFDFFFD;
    }
    else if (config.global_cfg.multitap_cfg == MT_ALT) {
        struct genesis_map *map3 = (struct genesis_map *)wired_adapter.data[2].output;
        struct genesis_map *map4 = (struct genesis_map *)wired_adapter.data[3].output;
        struct genesis_map *map3_mask = (struct genesis_map *)wired_adapter.data[2].output_mask;
        struct genesis_map *map4_mask = (struct genesis_map *)wired_adapter.data[3].output_mask;
        map2->buttons[0] = 0xFFFDFFFF;
        map2->buttons[1] = 0xFF79FFFF;
        map2->buttons[2] = 0xF379FFD7;
        map3->buttons[0] = 0xFFFDFFFF;
        map3->buttons[1] = 0xFF79FFFF;
        map3->buttons[2] = 0xF379FFD7;
        map4->buttons[0] = 0xFFFDFFFF;
        map4->buttons[1] = 0xFF79FFFF;
        map4->buttons[2] = 0xF379FFD7;
        memset(map3_mask->buttons, 0, sizeof(map3_mask->buttons));
        memset(map4_mask->buttons, 0, sizeof(map4_mask->buttons));
    }
    else if (config.out_cfg[1].dev_mode == DEV_MOUSE) {
        map2->buttons[0] = P2_MOUSE_ID0_HI;
        map2->buttons[1] = P2_MOUSE_ID0_LO;
        map2->buttons[2] = 0xFFFDFFFD;
    }
    else {
        map2->buttons[0] = 0xFFFDFFFD;
        map2->buttons[1] = 0xFDBDFFFD;
        map2->buttons[2] = 0xFFFDFFFD;
    }
    memset(map2_mask->buttons, 0, sizeof(map2_mask->buttons));

    map1->buttons_high[0] = 0xFFFFFFFE;
    map1->buttons_high[1] = 0xFFFFFFFE;
    map1->buttons_high[2] = 0xFFFFFFFE;
    map2->buttons_high[0] = 0xFFFFFFFE;
    map2->buttons_high[1] = 0xFFFFFFFE;
    map2->buttons_high[2] = 0xFFFFFFFE;
    memset(map1_mask->buttons_high, 0, sizeof(map1_mask->buttons_high));
    memset(map2_mask->buttons_high, 0, sizeof(map2_mask->buttons_high));

    if (dev_mode == DEV_MOUSE) {
        struct sega_mouse_map *map = (struct sega_mouse_map *)wired_data->output;
        map->twh_buttons = 0x00;
        for (uint32_t i = 0; i < 2; i++) {
            map->raw_axes[i] = 0;
            map->relative[i] = 1;
        }
    }
    else {
        struct genesis_map *map = (struct genesis_map *)wired_data->output;
        struct genesis_map *map_mask = (struct genesis_map *)wired_data->output_mask;
        map->twh_buttons = 0xFFFF;
        map_mask->twh_buttons = 0x0000;
    }
}

void genesis_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                //case DEV_KB:
                //    goto exit_axes_loop;
                case DEV_MOUSE:
                    ctrl_data[i].mask = sega_mouse_mask;
                    ctrl_data[i].desc = sega_mouse_desc;
                    ctrl_data[i].axes[j].meta = &sega_mouse_axes_meta[j];
                    break;
                default:
                    ctrl_data[i].mask = genesis_mask;
                    ctrl_data[i].desc = genesis_desc;
                    goto exit_axes_loop;
            }
        }
exit_axes_loop:
        ;
    }
}

void genesis_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        //case DEV_KB:
        //    break;
        case DEV_MOUSE:
            sega_mouse_from_generic(ctrl_data, wired_data);
            break;
        default:
            switch (ctrl_data->index) {
                case 0:
                    switch (config.global_cfg.multitap_cfg) {
                        case MT_SLOT_1:
                        case MT_DUAL:
                            genesis_twh_from_generic(ctrl_data, wired_data);
                            break;
                        case MT_ALT:
                            genesis_ea_from_generic(ctrl_data, wired_data);
                            break;
                        default:
                            genesis_std_from_generic(ctrl_data, wired_data);
                    }
                    break;
                case 1:
                    switch (config.global_cfg.multitap_cfg) {
                        case MT_SLOT_2:
                        case MT_DUAL:
                            genesis_twh_from_generic(ctrl_data, wired_data);
                            break;
                        case MT_ALT:
                            genesis_ea_from_generic(ctrl_data, wired_data);
                            break;
                        default:
                            genesis_std_from_generic(ctrl_data, wired_data);
                    }
                    break;
                case 2:
                    if (config.global_cfg.multitap_cfg == MT_ALT) {
                        genesis_ea_from_generic(ctrl_data, wired_data);
                    }
                    else {
                        genesis_twh_from_generic(ctrl_data, wired_data);
                    }
                    break;
                case 3:
                    if (config.global_cfg.multitap_cfg == MT_ALT) {
                        genesis_ea_from_generic(ctrl_data, wired_data);
                    }
                    else {
                        genesis_twh_from_generic(ctrl_data, wired_data);
                    }
                    break;
                default:
                    genesis_twh_from_generic(ctrl_data, wired_data);
            }
#if 0
            if (ctrl_data->index < 2) {
                struct genesis_map *map1 = (struct genesis_map *)wired_adapter.data[0].output;
                struct genesis_map *map2 = (struct genesis_map *)wired_adapter.data[1].output;
                uint32_t cycle = !(GPIO.in1.val & BIT(P1_TH_PIN - 32));
                uint32_t cycle2 = !(GPIO.in1.val & BIT(P2_TH_PIN - 32));
                GPIO.out = map1->buttons[cycle] & map2->buttons[cycle2];
                GPIO.out1.val = map1->buttons_high[cycle] & map2->buttons_high[cycle2];
            }
#endif
            break;
    }
}

void IRAM_ATTR genesis_gen_turbo_mask(uint32_t index, struct wired_data *wired_data) {
    struct genesis_map *map_mask = (struct genesis_map *)wired_data->output_mask;

    memset(map_mask, 0, sizeof(*map_mask));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        uint8_t mask = wired_data->cnt_mask[i] >> 1;

        if (mask) {
            for (uint32_t j = 0; j < 3; j++) {
                if (genesis_btns_mask[index][j][i]) {
                    if (wired_data->cnt_mask[i] & 1) {
                        if (!(mask & wired_data->frame_cnt)) {
                            if ((genesis_btns_mask[index][j][i] & 0xF0000000) == 0xF0000000) {
                                map_mask->buttons_high[j] |= genesis_btns_mask[index][j][i];
                            }
                            else {
                                map_mask->buttons[j] |= genesis_btns_mask[index][j][i];
                            }
                        }
                    }
                    else {
                        if (!((mask & wired_data->frame_cnt) == mask)) {
                            if ((genesis_btns_mask[index][j][i] & 0xF0000000) == 0xF0000000) {
                                map_mask->buttons_high[j] |= genesis_btns_mask[index][j][i];
                            }
                            else {
                                map_mask->buttons[j] |= genesis_btns_mask[index][j][i];
                            }
                        }
                    }
                }
            }
        }
    }
}

void IRAM_ATTR genesis_twh_gen_turbo_mask(struct wired_data *wired_data) {
    struct genesis_map *map_mask = (struct genesis_map *)wired_data->output_mask;

    map_mask->twh_buttons = 0x0000;

    wired_gen_turbo_mask_btns16_neg(wired_data, &map_mask->twh_buttons, genesis_twh_btns_mask);
}
