/*
 * Copyright (c) 2021-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include "hid_generic.h"
#include "ps3.h"
#include "wii.h"
#include "ps.h"
#include "xbox.h"
#include "sw.h"
#include "wireless.h"

static to_generic_t to_generic_func[BT_TYPE_MAX] = {
    hid_to_generic, /* BT_HID_GENERIC */
    ps3_to_generic, /* BT_PS3 */
    wii_to_generic, /* BT_WII */
    xbox_to_generic, /* BT_XBOX */
    ps_to_generic, /* BT_PS */
    sw_to_generic, /* BT_SW */
};

static fb_from_generic_t fb_from_generic_func[BT_TYPE_MAX] = {
    hid_fb_from_generic, /* BT_HID_GENERIC */
    ps3_fb_from_generic, /* BT_PS3 */
    wii_fb_from_generic, /* BT_WII */
    xbox_fb_from_generic, /* BT_XBOX */
    ps_fb_from_generic, /* BT_PS */
    sw_fb_from_generic, /* BT_SW */
};

int32_t wireless_to_generic(struct bt_data *bt_data, struct wireless_ctrl *ctrl_data) {
    int32_t ret = -1;

    if (to_generic_func[bt_data->base.pids->type]) {
        ret = to_generic_func[bt_data->base.pids->type](bt_data, ctrl_data);
    }

    return ret;
}

void wireless_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data) {
    if (fb_from_generic_func[bt_data->base.pids->type]) {
        fb_from_generic_func[bt_data->base.pids->type](fb_data, bt_data);
    }
}

