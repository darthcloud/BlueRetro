#include <stdio.h>
#include "bt_host.h"
#include "bt_hidp_sw.h"

static uint8_t sw_tid = 0;

void bt_hid_cmd_sw_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_sw_conf *sw_conf = (struct bt_hidp_sw_conf *)bt_hci_pkt_tmp.hidp_data;

    memcpy((void *)sw_conf, report, sizeof(*sw_conf));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_SW_SET_CONF, sizeof(*sw_conf));
}

void bt_hid_sw_init(struct bt_dev *device) {
    struct bt_hidp_sw_conf sw_conf = {
        .tid = sw_tid++,
        .subcmd = BT_HIDP_SW_SUBCMD_SET_LED,
        .subcmd_data[0] = bt_hid_led_dev_id_map[device->id],
    };
    printf("# %s\n", __FUNCTION__);

    bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
}

void bt_hid_sw_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_SW_SUBCMD_ACK:
                {
                    struct bt_hidp_sw_subcmd_ack *ack = (struct bt_hidp_sw_subcmd_ack *)bt_hci_acl_pkt->hidp_data;
                    printf("# BT_HIDP_SW_SUBCMD_ACK\n");
                    switch(ack->subcmd) {
                        case BT_HIDP_SW_SUBCMD_SET_LED:
                        {
                            struct bt_hidp_sw_conf sw_conf = {
                                .tid = sw_tid++,
                                .subcmd = BT_HIDP_SW_SUBCMD_EN_RUMBLE,
                                .subcmd_data[0] = 0x01,
                            };
                            bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_EN_RUMBLE:
                        {
                            struct bt_hidp_sw_conf sw_conf = {
                                .tid = sw_tid++,
                                .subcmd = BT_HIDP_SW_SUBCMD_SET_REP_MODE,
                                .subcmd_data[0] = BT_HIDP_SW_STATUS,
                            };
                            bt_hid_cmd_sw_set_conf(device, (void *)&sw_conf);
                            break;
                        }
                        case BT_HIDP_SW_SUBCMD_SET_REP_MODE:
                        {
                            break;
                        }
                    }
                    break;
                }
                case BT_HIDP_SW_STATUS:
                {
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_data, sizeof(struct bt_hidp_sw_status));
                    break;
                }
            }
            break;
    }
}
