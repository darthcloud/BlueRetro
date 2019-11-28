#include <stdio.h>
#include "host.h"
#include "hidp_ps3.h"

static const uint8_t bt_init_magic[] = {
    0x42, 0x03, 0x00, 0x00
};

static const uint8_t config[] = {
    0x01, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x02, 0xff, 0x27, 0x10, 0x00, 0x32, 0xff,
    0x27, 0x10, 0x00, 0x32, 0xff, 0x27, 0x10, 0x00,
    0x32, 0xff, 0x27, 0x10, 0x00, 0x32, 0x00, 0x00,
    0x00, 0x00, 0x00
};

static void bt_hid_cmd_ps3_bt_init(struct bt_dev *device) {
    struct bt_hidp_ps3_bt_init *bt_init = (struct bt_hidp_ps3_bt_init *)bt_hci_pkt_tmp.hidp_data;

    memcpy((void *)bt_init, bt_init_magic, sizeof(*bt_init));

    bt_hid_cmd(device->acl_handle, device->ctrl_chan.dcid, BT_HIDP_SET_FE, BT_HIDP_PS3_BT_INIT, sizeof(*bt_init));
}

void bt_hid_cmd_ps3_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_ps3_set_conf *set_conf = (struct bt_hidp_ps3_set_conf *)bt_hci_pkt_tmp.hidp_data;

    memcpy((void *)set_conf, report, sizeof(*set_conf));

    bt_hid_cmd(device->acl_handle, device->ctrl_chan.dcid, BT_HIDP_SET_OUT, BT_HIDP_PS3_SET_CONF, sizeof(*set_conf));
}

void bt_hid_ps3_init(struct bt_dev *device) {
    struct bt_hidp_ps3_set_conf set_conf;
    memcpy((void *)&set_conf, config, sizeof(set_conf));
    set_conf.leds = (bt_hid_led_dev_id_map[device->id] << 1);

    printf("# %s\n", __FUNCTION__);

    bt_hid_cmd_ps3_bt_init(device);
    bt_hid_cmd_ps3_set_conf(device, (void *)&set_conf);
}

void bt_hid_ps3_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_PS3_STATUS:
                {
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_hdr.protocol, bt_hci_acl_pkt->hidp_data, sizeof(struct bt_hidp_ps3_status));
                    break;
                }
            }
            break;
    }
}
