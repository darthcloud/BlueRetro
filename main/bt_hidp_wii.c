#include <stdio.h>
#include "bt_host.h"
#include "bt_hidp_wii.h"

struct bt_wii_ext_type {
    uint8_t ext_type[6];
    int8_t type;
};

static const struct bt_hidp_wii_rep_mode wii_rep_conf = {
    BT_HIDP_WII_CONTINUOUS,
    BT_HIDP_WII_CORE_ACC_EXT,
};

static const struct bt_hidp_wii_wr_mem wii_ext_init0 = {
    BT_HIDP_WII_REG,
    {0xA4, 0x00, 0xF0},
    0x01,
    {0x55},
};

static const struct bt_hidp_wii_wr_mem wii_ext_init1 = {
    BT_HIDP_WII_REG,
    {0xA4, 0x00, 0xFB},
    0x01,
    {0x00},
};

static const struct bt_hidp_wii_rd_mem wii_ext_type = {
    BT_HIDP_WII_REG,
    {0xA4, 0x00, 0xFA},
    0x0600,
};

static const struct bt_wii_ext_type bt_wii_ext_type[] = {
    {{0x00, 0x00, 0xA4, 0x20, 0x00, 0x00}, WII_NUNCHUCK},
    {{0x00, 0x00, 0xA4, 0x20, 0x01, 0x01}, WII_CLASSIC},
    {{0x01, 0x00, 0xA4, 0x20, 0x01, 0x01}, WII_CLASSIC}, /* Classic Pro */
    {{0x00, 0x00, 0xA4, 0x20, 0x01, 0x20}, WIIU_PRO},
};

static int32_t bt_get_type_from_wii_ext(const uint8_t* ext_type);
static void bt_hid_cmd_wii_set_rep_mode(struct bt_dev *device, void *report);
static void bt_hid_cmd_wii_read(struct bt_dev *device, void *report);
static void bt_hid_cmd_wii_write(struct bt_dev *device, void *report);

static int32_t bt_get_type_from_wii_ext(const uint8_t* ext_type) {
    for (uint32_t i = 0; i < sizeof(bt_wii_ext_type)/sizeof(*bt_wii_ext_type); i++) {
        if (memcmp(ext_type, bt_wii_ext_type[i].ext_type, sizeof(bt_wii_ext_type[0].ext_type)) == 0) {
            return bt_wii_ext_type[i].type;
        }
    }
    return BT_NONE;
}

int32_t bt_dev_is_wii(int8_t type) {
    switch (type) {
        case WII_CORE:
        case WII_NUNCHUCK:
        case WII_CLASSIC:
        case WIIU_PRO:
            return 1;
        default:
            return 0;
    }
}

static void bt_hid_cmd_wii_set_rep_mode(struct bt_dev *device, void *report) {
    struct bt_hidp_wii_rep_mode *wii_rep_mode = (struct bt_hidp_wii_rep_mode *)bt_hci_pkt_tmp.hidp_data;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_rep_mode, report, sizeof(*wii_rep_mode));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_WII_REP_MODE, sizeof(*wii_rep_mode));
}

static void bt_hid_cmd_wii_read(struct bt_dev *device, void *report) {
    struct bt_hidp_wii_rd_mem *wii_rd_mem = (struct bt_hidp_wii_rd_mem *)bt_hci_pkt_tmp.hidp_data;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_rd_mem, report, sizeof(*wii_rd_mem));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_WII_RD_MEM, sizeof(*wii_rd_mem));
}

static void bt_hid_cmd_wii_write(struct bt_dev *device, void *report) {
    struct bt_hidp_wii_wr_mem *wii_wr_mem = (struct bt_hidp_wii_wr_mem *)bt_hci_pkt_tmp.hidp_data;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_wr_mem, report, sizeof(*wii_wr_mem));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_WII_WR_MEM, sizeof(*wii_wr_mem));
}

void bt_hid_cmd_wii_set_feedback(struct bt_dev *device, void *report) {
    struct bt_hidp_wii_conf *wii_conf = (struct bt_hidp_wii_conf *)bt_hci_pkt_tmp.hidp_data;
    //printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_conf, report, sizeof(*wii_conf));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_WII_LED_REPORT, sizeof(*wii_conf));
}

void bt_hid_wii_init(struct bt_dev *device) {
    struct bt_hidp_wii_conf wii_conf = {.conf = (bt_hid_led_dev_id_map[device->id] << 4)};
    printf("# %s\n", __FUNCTION__);

    bt_hid_cmd_wii_set_feedback(device, (void *)&wii_conf);
    bt_hid_cmd_wii_set_rep_mode(device, (void *)&wii_rep_conf);
}

void bt_hid_wii_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_WII_STATUS:
                {
                    struct bt_hidp_wii_status *status = (struct bt_hidp_wii_status *)bt_hci_acl_pkt->hidp_data;
                    printf("# BT_HIDP_WII_STATUS\n");
                    if (device->type != WIIU_PRO) {
                        device->type = WII_CORE;
                        if (status->flags & BT_HIDP_WII_FLAGS_EXT_CONN) {
                            bt_hid_cmd_wii_write(device, (void *)&wii_ext_init0);
                        }
                        else {
                            bt_hid_cmd_wii_set_rep_mode(device, (void *)&wii_rep_conf);
                        }
                    }
                    break;
                }
                case BT_HIDP_WII_RD_DATA:
                {
                    struct bt_hidp_wii_rd_data *rd_data = (struct bt_hidp_wii_rd_data *)bt_hci_acl_pkt->hidp_data;
                    int8_t type = bt_get_type_from_wii_ext(rd_data->data);
                    printf("# BT_HIDP_WII_RD_DATA\n");
                    if (type > BT_NONE) {
                        device->type = type;
                    }
                    printf("# dev: %d wii ext: %d\n", device->id, device->type);
                    bt_hid_cmd_wii_set_rep_mode(device, (void *)&wii_rep_conf);
                    break;
                }
                case BT_HIDP_WII_ACK:
                {
                    struct bt_hidp_wii_ack *ack = (struct bt_hidp_wii_ack *)bt_hci_acl_pkt->hidp_data;
                    printf("# BT_HIDP_WII_ACK\n");
                    if (ack->err) {
                        printf("# dev: %d ack err: 0x%02X\n", device->id, ack->err);
                        if (device->hid_state) {
                            bt_hid_cmd_wii_write(device, (void *)&wii_ext_init1);
                        }
                        else {
                            bt_hid_cmd_wii_write(device, (void *)&wii_ext_init0);
                        }
                    }
                    else {
                        device->hid_state ^= 0x01;
                        switch(ack->report) {
                            case BT_HIDP_WII_WR_MEM:
                                if (device->hid_state) {
                                    bt_hid_cmd_wii_write(device, (void *)&wii_ext_init1);
                                }
                                else {
                                    bt_hid_cmd_wii_read(device, (void *)&wii_ext_type);
                                }
                                break;
                        }
                    }
                    break;
                }
                case BT_HIDP_WII_CORE_ACC_EXT:
                {
                    device->report_cnt++;
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_data, sizeof(struct bt_hidp_wii_core_acc_ext));
#if 0
                    struct wiiu_pro_map *wiiu_pro = (struct wiiu_pro_map *)&bt_hci_acl_packet->pl.hidp.hidp_data.wii_core_acc_ext.ext;
                    device->report_cnt++;
                    input.format = device->type;
                    if (atomic_test_bit(&bt_flags, BT_CTRL_READY) && atomic_test_bit(&input.flags, BTIO_UPDATE_CTRL)) {
                        bt_hid_cmd_wii_set_led(device->acl_handle, device->intr_chan.dcid, input.leds_rumble);
                        atomic_clear_bit(&input.flags, BTIO_UPDATE_CTRL);
                    }
                    memcpy(&input.io.wiiu_pro, wiiu_pro, sizeof(*wiiu_pro));
                    translate_status(sd_config, &input, output);
                    min_lx = min(min_lx, wiiu_pro->axes[0]);
                    min_ly = min(min_ly, wiiu_pro->axes[2]);
                    min_rx = min(min_rx, wiiu_pro->axes[1]);
                    min_ry = min(min_ry, wiiu_pro->axes[3]);
                    max_lx = max(max_lx, wiiu_pro->axes[0]);
                    max_ly = max(max_ly, wiiu_pro->axes[2]);
                    max_rx = max(max_rx, wiiu_pro->axes[1]);
                    max_ry = max(max_ry, wiiu_pro->axes[3]);
                    printf("JG2019 MIN LX 0x%04X LY 0x%04X RX 0x%04X RY 0x%04X\n", min_lx, min_ly, min_rx, min_ry);
                    printf("JG2019 MAX LX 0x%04X LY 0x%04X RX 0x%04X RY 0x%04X\n", max_lx, max_ly, max_rx, max_ry);
                    printf("JG2019 %04X %04X %04X %04X\n", wiiu_pro->axes[0] - 0x800, wiiu_pro->axes[2] - 0x800, wiiu_pro->axes[1] - 0x800, wiiu_pro->axes[3] - 0x800);
#endif
                    break;
                }
            }
            break;
    }
}
