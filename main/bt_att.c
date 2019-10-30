#include <stdio.h>
#include "bt_host.h"
#include "bt_att.h"

void bt_att_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->att_hdr.code) {
    }
}
