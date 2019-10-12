#include <stdio.h>
#include "adapter.h"
#include "bt_host.h"
#include "bt_hidp_wii.h"

struct bt_wii_ext_type {
    uint8_t ext_type[6];
    int8_t type;
};

static struct bt_hidp_wii_rep_mode wii_rep_conf =
{
    BT_HIDP_WII_CONTINUOUS,
    BT_HIDP_WII_CORE_ACC_EXT
};

static struct bt_hidp_wii_wr_mem wii_ext_init0 =
{
    BT_HIDP_WII_REG,
    {0xA4, 0x00, 0xF0},
    0x01,
    {0x55}
};

static struct bt_hidp_wii_wr_mem wii_ext_init1 =
{
    BT_HIDP_WII_REG,
    {0xA4, 0x00, 0xFB},
    0x01,
    {0x00}
};

static struct bt_hidp_wii_rd_mem wii_ext_type =
{
    BT_HIDP_WII_REG,
    {0xA4, 0x00, 0xFA},
    0x0600,
};

static const struct bt_wii_ext_type bt_wii_ext_type[] = {
    {{0x00, 0x00, 0xA4, 0x20, 0x00, 0x00}, WII_NUNCHUCK},
    {{0x00, 0x00, 0xA4, 0x20, 0x01, 0x01}, WII_CLASSIC},
    {{0x01, 0x00, 0xA4, 0x20, 0x01, 0x01}, WII_CLASSIC}, /* Classic Pro */
    {{0x00, 0x00, 0xA4, 0x20, 0x01, 0x20}, WIIU_PRO}
};

struct bt_hidp_cmd bt_hipd_wii_conf[8] =
{
    {bt_hid_cmd_wii_set_user_led, NULL},
    {bt_hid_cmd_wii_set_rep_mode, &wii_rep_conf},
    {bt_hid_cmd_wii_write, &wii_ext_init0},
    {bt_hid_cmd_wii_write, &wii_ext_init1},
    {bt_hid_cmd_wii_read, &wii_ext_type}
};

int32_t bt_get_type_from_wii_ext(const uint8_t* ext_type) {
    for (uint32_t i = 0; i < sizeof(bt_wii_ext_type)/sizeof(*bt_wii_ext_type); i++) {
        if (memcmp(ext_type, bt_wii_ext_type[i].ext_type, sizeof(bt_wii_ext_type[0].ext_type)) == 0) {
            return bt_wii_ext_type[i].type;
        }
    }
    return -1;
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

void bt_hid_cmd_wii_set_feedback(void *bt_dev, void *report) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    struct bt_hidp_wii_conf *wii_conf = (struct bt_hidp_wii_conf *)bt_hci_pkt_tmp.hidp_data;
    //printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_conf, report, sizeof(*wii_conf));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_WII_LED_REPORT, sizeof(*wii_conf));
}

void bt_hid_cmd_wii_set_user_led(void *bt_dev, void *report) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    struct bt_hidp_wii_conf wii_conf = {.conf = (led_dev_id_map[device->id] << 4)};
    bt_hid_cmd_wii_set_feedback(bt_dev, (void *)&wii_conf);
}

void bt_hid_cmd_wii_set_rep_mode(void *bt_dev, void *report) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    struct bt_hidp_wii_rep_mode *wii_rep_mode = (struct bt_hidp_wii_rep_mode *)bt_hci_pkt_tmp.hidp_data;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_rep_mode, report, sizeof(*wii_rep_mode));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_WII_REP_MODE, sizeof(*wii_rep_mode));
}

void bt_hid_cmd_wii_read(void *bt_dev, void *report) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    struct bt_hidp_wii_rd_mem *wii_rd_mem = (struct bt_hidp_wii_rd_mem *)bt_hci_pkt_tmp.hidp_data;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_rd_mem, report, sizeof(*wii_rd_mem));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_WII_RD_MEM, sizeof(*wii_rd_mem));
}

void bt_hid_cmd_wii_write(void *bt_dev, void *report) {
    struct bt_dev *device = (struct bt_dev *)bt_dev;
    struct bt_hidp_wii_wr_mem *wii_wr_mem = (struct bt_hidp_wii_wr_mem *)bt_hci_pkt_tmp.hidp_data;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_wr_mem, report, sizeof(*wii_wr_mem));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_WII_WR_MEM, sizeof(*wii_wr_mem));
}
