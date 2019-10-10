#include <stdio.h>
#include "bt_host.h"

void bt_hid_cmd_wii_set_led(uint16_t handle, uint16_t cid, uint8_t conf) {
    struct bt_hidp_wii_conf *wii_conf = (struct bt_hidp_wii_conf *)bt_hci_pkt_tmp.hidp_data;
    //printf("# %s\n", __FUNCTION__);

    wii_conf->conf = conf;

    bt_hid_cmd(handle, cid, BT_HIDP_WII_LED_REPORT, sizeof(*wii_conf));
}

void bt_hid_cmd_wii_set_rep_mode(uint16_t handle, uint16_t cid, uint8_t continuous, uint8_t mode) {
    struct bt_hidp_wii_rep_mode *wii_rep_mode = (struct bt_hidp_wii_rep_mode *)bt_hci_pkt_tmp.hidp_data;
    printf("# %s\n", __FUNCTION__);

    wii_rep_mode->options = (continuous ? 0x04 : 0x00);
    wii_rep_mode->mode = mode;

    bt_hid_cmd(handle, cid, BT_HIDP_WII_REP_MODE, sizeof(*wii_rep_mode));
}

void bt_hid_cmd_wii_read(uint16_t handle, uint16_t cid, struct bt_hidp_wii_rd_mem *data) {
    struct bt_hidp_wii_rd_mem *wii_rd_mem = (struct bt_hidp_wii_rd_mem *)bt_hci_pkt_tmp.hidp_data;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_rd_mem, (void *)data, sizeof(*wii_rd_mem));

    bt_hid_cmd(handle, cid, BT_HIDP_WII_RD_MEM, sizeof(*wii_rd_mem));
}

void bt_hid_cmd_wii_write(uint16_t handle, uint16_t cid, struct bt_hidp_wii_wr_mem *data) {
    struct bt_hidp_wii_wr_mem *wii_wr_mem = (struct bt_hidp_wii_wr_mem *)bt_hci_pkt_tmp.hidp_data;
    printf("# %s\n", __FUNCTION__);

    memcpy((void *)wii_wr_mem, (void *)data, sizeof(*wii_wr_mem));

    bt_hid_cmd(handle, cid, BT_HIDP_WII_WR_MEM, sizeof(*wii_wr_mem));
}
