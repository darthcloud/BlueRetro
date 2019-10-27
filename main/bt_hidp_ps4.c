#include <stdio.h>
#include <esp32/rom/crc.h>
#include "bt_host.h"
#include "bt_hidp_ps4.h"

static const uint8_t bt_ps4_led_dev_id_map[][3] = {
    {0x00, 0x00, 0x40},
    {0x40, 0x00, 0x00},
    {0x00, 0x40, 0x00},
    {0x20, 0x00, 0x20},
    {0x02, 0x01, 0x00},
    {0x00, 0x01, 0x01},
    {0x01, 0x01, 0x01},
};

void bt_hid_cmd_ps4_set_conf(struct bt_dev *device, void *report) {
    struct bt_hidp_ps4_set_conf *set_conf = (struct bt_hidp_ps4_set_conf *)bt_hci_pkt_tmp.hidp_data;

    bt_hci_pkt_tmp.hidp_hdr.hdr = BT_HIDP_DATA_OUT;
    bt_hci_pkt_tmp.hidp_hdr.protocol = BT_HIDP_PS4_SET_CONF;

    memcpy((void *)set_conf, report, sizeof(*set_conf));

    set_conf->crc = crc32_le((uint32_t)~0xFFFFFFFF, (void *)&bt_hci_pkt_tmp.hidp_hdr,
        sizeof(bt_hci_pkt_tmp.hidp_hdr) + sizeof(*set_conf) - sizeof(set_conf->crc));

    bt_hid_cmd(device->acl_handle, device->intr_chan.dcid, BT_HIDP_DATA_OUT, BT_HIDP_PS4_SET_CONF, sizeof(*set_conf));
}

void bt_hid_ps4_init(struct bt_dev *device) {
    struct bt_hidp_ps4_set_conf set_conf = {
        .conf0 = 0xc4,
        .conf1 = 0x07,
    };

    memcpy(set_conf.leds, bt_ps4_led_dev_id_map[device->id], sizeof(set_conf.leds));

    printf("# %s\n", __FUNCTION__);

    bt_hid_cmd_ps4_set_conf(device, (void *)&set_conf);
}

void bt_hid_ps4_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->sig_hdr.code) {
        case BT_HIDP_DATA_IN:
            switch (bt_hci_acl_pkt->hidp_hdr.protocol) {
                case BT_HIDP_PS4_STATUS:
                case BT_HIDP_PS4_STATUS2:
                {
                    device->report_cnt++;
                    bt_host_bridge(device, bt_hci_acl_pkt->hidp_data, sizeof(struct bt_hidp_ps4_status));
                    break;
                }
            }
            break;
    }
}
