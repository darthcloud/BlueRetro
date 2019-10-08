#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include <esp_bt.h>
#include <nvs_flash.h>
#include "zephyr/hci.h"
#include "zephyr/l2cap_internal.h"
#include "zephyr/atomic.h"
#include "hidp.h"
#include "adapter.h"

#ifdef WIP
static void bt_hid_cmd(uint16_t handle, uint16_t cid, uint8_t protocol, uint16_t len) {
    uint16_t buflen = (sizeof(bt_acl_frame.h4_type) + sizeof(bt_acl_frame.acl_hdr)
                       + sizeof(bt_acl_frame.l2cap_hdr) + sizeof(bt_acl_frame.pl.hidp.hidp_hdr) + len);

    atomic_clear_bit(&bt_flags, BT_CTRL_READY);

    bt_acl_frame.h4_type = H4_TYPE_ACL;

    bt_acl_frame.acl_hdr.handle = handle | (0x2 << 12); /* BC/PB Flags (2 bits each) */
    bt_acl_frame.acl_hdr.len = buflen - sizeof(bt_acl_frame.h4_type) - sizeof(bt_acl_frame.acl_hdr);

    bt_acl_frame.l2cap_hdr.len = bt_acl_frame.acl_hdr.len - sizeof(bt_acl_frame.l2cap_hdr);
    bt_acl_frame.l2cap_hdr.cid = cid;

    bt_acl_frame.pl.hidp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_acl_frame.pl.hidp.hidp_hdr.protocol = protocol;

#ifdef H4_TRACE
    bt_h4_trace((uint8_t *)&bt_acl_frame, buflen, BT_TX);
#endif /* H4_TRACE */

    esp_vhci_host_send_packet((uint8_t *)&bt_acl_frame, buflen);
}

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
