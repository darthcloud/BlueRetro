#include "bt_host.h"

#ifdef WIP
static void bt_hid_cmd_wii_set_led(uint16_t handle, uint16_t cid, uint8_t conf) {
    //printf("# %s\n", __FUNCTION__);

    bt_acl_frame.pl.hidp.hidp_data.wii_conf.conf = conf;

    bt_hid_cmd(handle, cid, BT_HIDP_WII_LED_REPORT, sizeof(bt_acl_frame.pl.hidp.hidp_data.wii_conf));
}

static void bt_hid_cmd_wii_set_rep_mode(uint16_t handle, uint16_t cid, uint8_t continuous, uint8_t mode) {
    printf("# %s\n", __FUNCTION__);

    bt_acl_frame.pl.hidp.hidp_data.wii_rep_mode.options = (continuous ? 0x04 : 0x00);
    bt_acl_frame.pl.hidp.hidp_data.wii_rep_mode.mode = mode;

    bt_hid_cmd(handle, cid, BT_HIDP_WII_REP_MODE, sizeof(bt_acl_frame.pl.hidp.hidp_data.wii_rep_mode));
}

static void bt_hid_cmd_wii_read(uint16_t handle, uint16_t cid, struct bt_hidp_wii_rd_mem *wii_rd_mem) {
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&bt_acl_frame.pl.hidp.hidp_data.wii_rd_mem, (void *)wii_rd_mem, sizeof(bt_acl_frame.pl.hidp.hidp_data.wii_rd_mem));

    bt_hid_cmd(handle, cid, BT_HIDP_WII_RD_MEM, sizeof(bt_acl_frame.pl.hidp.hidp_data.wii_rd_mem));
}

static void bt_hid_cmd_wii_write(uint16_t handle, uint16_t cid, struct bt_hidp_wii_wr_mem *wii_wr_mem) {
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)&bt_acl_frame.pl.hidp.hidp_data.wii_wr_mem, (void *)wii_wr_mem, sizeof(bt_acl_frame.pl.hidp.hidp_data.wii_wr_mem));

    bt_hid_cmd(handle, cid, BT_HIDP_WII_WR_MEM, sizeof(bt_acl_frame.pl.hidp.hidp_data.wii_wr_mem));
}
#endif
