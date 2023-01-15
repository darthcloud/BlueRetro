/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bluetooth/host.h"
#include "tools/util.h"
#include "generic.h"
#include "ps3.h"
#include "wii.h"
#include "ps.h"
#include "xbox.h"
#include "sw.h"

typedef void (*bt_hid_init_t)(struct bt_dev *device);
typedef void (*bt_hid_hdlr_t)(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);
typedef void (*bt_hid_cmd_t)(struct bt_dev *device, void *report);

const uint8_t bt_hid_led_dev_id_map[] = {
    0x1, 0x2, 0x4, 0x8, 0x3, 0x6, 0xC
};

static const struct bt_name_type bt_name_type[] = {
#ifndef CONFIG_BLUERETRO_GENERIC_HID_DEBUG
    {"PLAYSTATION(R)3", BT_PS3, BT_SUBTYPE_DEFAULT, 0},
    {"Xbox Wireless Controller", BT_XBOX, BT_XBOX_XINPUT, 0},
    {"Xbox Adaptive Controller", BT_XBOX, BT_XBOX_ADAPTIVE, 0},
    {"Xbox Wireless Contr", BT_XBOX, BT_XBOX_XS, 0},
    {"Wireless Controller", BT_PS, BT_SUBTYPE_DEFAULT, 0},
    {"Nintendo RVL-CNT-01-UC", BT_WII, BT_WIIU_PRO, 0}, /* Must be before WII */
    {"Nintendo RVL-CNT-01", BT_WII, BT_SUBTYPE_DEFAULT, 0},
    {"Lic Pro Controller", BT_SW, BT_SW_POWERA, BIT(BT_QUIRK_FACE_BTNS_ROTATE_RIGHT)},
    {"Pro Controller", BT_SW, BT_SUBTYPE_DEFAULT, 0},
    {"Joy-Con (L)", BT_SW, BT_SW_LEFT_JOYCON, BIT(BT_QUIRK_SW_LEFT_JOYCON)},
    {"Joy-Con (R)", BT_SW, BT_SW_RIGHT_JOYCON, BIT(BT_QUIRK_SW_RIGHT_JOYCON)},
    {"HVC Controller", BT_SW, BT_SW_NES, BIT(BT_QUIRK_FACE_BTNS_ROTATE_RIGHT) | BIT(BT_QUIRK_TRIGGER_PRI_SEC_INVERT)},
    {"NES Controller", BT_SW, BT_SW_NES, BIT(BT_QUIRK_FACE_BTNS_ROTATE_RIGHT) | BIT(BT_QUIRK_TRIGGER_PRI_SEC_INVERT)},
    {"SNES Controller", BT_SW, BT_SW_SNES, BIT(BT_QUIRK_TRIGGER_PRI_SEC_INVERT)},
    {"N64 Controller", BT_SW, BT_SW_N64, 0},
    {"MD/Gen Control Pad", BT_SW, BT_SW_MD_GEN, 0},
    {"8BitDo N30 Modkit", BT_XBOX, BT_XBOX_XINPUT, BIT(BT_QUIRK_FACE_BTNS_ROTATE_RIGHT)},
    {"8BitDo GBros Adapter", BT_XBOX, BT_8BITDO_GBROS, 0},
    {"8Bitdo N64 GamePad", BT_HID_GENERIC, BT_SUBTYPE_DEFAULT, BIT(BT_QUIRK_8BITDO_N64)},
    {"8BitDo M30 gamepad", BT_XBOX, BT_XBOX_XINPUT, BIT(BT_QUIRK_8BITDO_M30)},
    {"8BitDo S30 Modkit", BT_XBOX, BT_XBOX_XINPUT, BIT(BT_QUIRK_8BITDO_SATURN)},
    {"8Bitdo", BT_XBOX, BT_XBOX_XINPUT, 0}, /* 8bitdo catch all, tested with SF30 Pro */
    {"Retro Bit Bluetooth Controller", BT_XBOX, BT_XBOX_XINPUT, BIT(BT_QUIRK_FACE_BTNS_TRIGGER_TO_6BUTTONS) | BIT(BT_QUIRK_TRIGGER_PRI_SEC_INVERT)},
    {"Joy Controller", BT_XBOX, BT_XBOX_XINPUT, BIT(BT_QUIRK_RF_WARRIOR)},
    {"BlueN64 Gamepad", BT_HID_GENERIC, BT_SUBTYPE_DEFAULT, BIT(BT_QUIRK_BLUEN64_N64)},
    {"Hyperkin Pad", BT_SW, BT_SW_HYPERKIN_ADMIRAL, 0},
#endif
};

static const bt_hid_init_t bt_hid_init_list[BT_TYPE_MAX] = {
    NULL, /* BT_HID_GENERIC */
    bt_hid_ps3_init, /* BT_PS3 */
    bt_hid_wii_init, /* BT_WII */
    bt_hid_xbox_init, /* BT_XBOX */
    bt_hid_ps_init, /* BT_PS */
    bt_hid_sw_init, /* BT_SW */
};

static const bt_hid_hdlr_t bt_hid_hdlr_list[BT_TYPE_MAX] = {
    bt_hid_generic_hdlr, /* BT_HID_GENERIC */
    bt_hid_ps3_hdlr, /* BT_PS3 */
    bt_hid_wii_hdlr, /* BT_WII */
    bt_hid_xbox_hdlr, /* BT_XBOX */
    bt_hid_ps_hdlr, /* BT_PS */
    bt_hid_sw_hdlr, /* BT_SW */
};

static const bt_hid_cmd_t bt_hid_feedback_list[BT_TYPE_MAX] = {
    NULL, /* BT_HID_GENERIC */
    bt_hid_cmd_ps3_set_conf, /* BT_PS3 */
    bt_hid_cmd_wii_set_feedback, /* BT_WII */
    bt_hid_cmd_xbox_rumble, /* BT_XBOX */
    bt_hid_cmd_ps_set_conf, /* BT_PS */
    bt_hid_cmd_sw_set_conf, /* BT_SW */
};

void bt_hid_set_type_flags_from_name(struct bt_dev *device, const char* name) {
    for (uint32_t i = 0; i < sizeof(bt_name_type)/sizeof(*bt_name_type); i++) {
        if (strstr(name, bt_name_type[i].name) != NULL) {
            struct bt_data *bt_data = &bt_adapter.data[device->ids.id];

            bt_type_update(device->ids.id, bt_name_type[i].type, bt_name_type[i].subtype);
            bt_data->base.flags[PAD] = bt_name_type[i].hid_flags;
            device->name = &bt_name_type[i];
            break;
        }
    }
}

void bt_hid_init(struct bt_dev *device) {
    if (device->ids.type > BT_NONE && bt_hid_init_list[device->ids.type]) {
        bt_hid_init_list[device->ids.type](device);
    }
    atomic_set_bit(&device->flags, BT_DEV_HID_INIT_DONE);
}

void bt_hid_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    if (device->ids.type > BT_NONE && bt_hid_hdlr_list[device->ids.type]) {
        bt_hid_hdlr_list[device->ids.type](device, bt_hci_acl_pkt, len);
    }
}

void bt_hid_feedback(struct bt_dev *device, void *report) {
    if (device->ids.type > BT_NONE && bt_hid_feedback_list[device->ids.type]) {
        bt_hid_feedback_list[device->ids.type](device, report);
    }
}

void bt_hid_cmd(uint16_t handle, uint16_t cid, uint8_t hidp_hdr, uint8_t protocol, uint16_t len) {
    uint16_t packet_len = (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
        + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_hidp_hdr) + len);

    bt_hci_pkt_tmp.h4_hdr.type = BT_HCI_H4_TYPE_ACL;

    bt_hci_pkt_tmp.acl_hdr.handle = bt_acl_handle_pack(handle, 0x2);
    bt_hci_pkt_tmp.acl_hdr.len = packet_len - BT_HCI_H4_HDR_SIZE - BT_HCI_ACL_HDR_SIZE;

    bt_hci_pkt_tmp.l2cap_hdr.len = bt_hci_pkt_tmp.acl_hdr.len - sizeof(bt_hci_pkt_tmp.l2cap_hdr);
    bt_hci_pkt_tmp.l2cap_hdr.cid = cid;

    bt_hci_pkt_tmp.hidp_hdr.hdr = hidp_hdr;
    bt_hci_pkt_tmp.hidp_hdr.protocol = protocol;

    bt_host_txq_add((uint8_t *)&bt_hci_pkt_tmp, packet_len);
}
