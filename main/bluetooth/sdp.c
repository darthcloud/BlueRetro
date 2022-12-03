/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "zephyr/types.h"
#include "zephyr/sdp.h"
#include "tools/util.h"
#include "host.h"
#include "adapter/hid_parser.h"
#include "l2cap.h"
#include "sdp.h"

/* We only care about HID Descriptor and maybe PNP vendor/product ID */
/* But safer to request all L2CAP attribute like BlueZ do */
#define SDP_GET_ALL_L2CAP_ATTR 1

#ifdef SDP_GET_ALL_L2CAP_ATTR
static const uint8_t l2cap_attr_req[] = {
    /* Service Search Pattern */
        /* Data Element */
            /* Type */ BT_SDP_SEQ8,
            /* Size */ 0x03,
            /* Data Value */
                /* Data Element */
                    /* Type */ BT_SDP_UUID16,
                    /* Data Value */
                        /* UUID */ 0x01, 0x00, /* L2CAP */
    /* Max Att Byte */ 0x04, 0x00, /* 1024 */
    /* Att ID List */
        /* Data Element */
            /* Type */ BT_SDP_SEQ8,
            /* Size */ 0x05,
            /* Data Value */
                /* Data Element */
                    /* Type */ BT_SDP_UINT32,
                    /* Data Value */
                        /* Att Range */ 0x00, 0x00, /* to */ 0xff, 0xff,
};
#else
static const uint8_t hid_descriptor_attr_req[] = {
    /* Service Search Pattern */
        /* Data Element */
            /* Type */ BT_SDP_SEQ8,
            /* Size */ 0x03,
            /* Data Value */
                /* Data Element */
                    /* Type */ BT_SDP_UUID16,
                    /* Data Value */
                        /* UUID */ (BT_SDP_HID_SVCLASS >> 8), (BT_SDP_HID_SVCLASS & 0xff),
    /* Max Att Byte */ 0x04, 0x00, /* 1024 */
    /* Att ID List */
        /* Data Element */
            /* Type */ BT_SDP_SEQ8,
            /* Size */ 0x03,
            /* Data Value */
                /* Data Element */
                    /* Type */ BT_SDP_UUID16,
                    /* Data Value */
                        /* Att */ (BT_SDP_ATTR_HID_DESCRIPTOR_LIST >> 8), (BT_SDP_ATTR_HID_DESCRIPTOR_LIST & 0xff),
};

static const uint8_t pnp_attr_req[] = {
    /* Service Search Pattern */
        /* Data Element */
            /* Type */ BT_SDP_SEQ8,
            /* Size */ 0x03,
            /* Data Value */
                /* Data Element */
                    /* Type */ BT_SDP_UUID16,
                    /* Data Value */
                        /* UUID */ (BT_SDP_PNP_INFO_SVCLASS >> 8), (BT_SDP_PNP_INFO_SVCLASS & 0xff),
    /* Max Att Byte */ 0x04, 0x00, /* 1024 */
    /* Att ID List */
        /* Data Element */
            /* Type */ BT_SDP_SEQ8,
            /* Size */ 0x05,
            /* Data Value */
                /* Data Element */
                    /* Type */ BT_SDP_UINT32,
                    /* Data Value */
                        /* Att Range */ (BT_SDP_ATTR_VENDOR_ID >> 8), (BT_SDP_ATTR_VENDOR_ID & 0xff),
                            /* to */    (BT_SDP_ATTR_PRODUCT_ID >> 8), (BT_SDP_ATTR_PRODUCT_ID & 0xff),
};
#endif

static const uint8_t xb1_svc_search_attr_rsp[] = {
    0x35, 0x0a, 0x35, 0x08, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x12, 0x00
};

static const uint8_t xb1_svc_search_rsp[] = {
    0x00, 0x01, 0x00, 0x00
};

static const uint8_t xb1_svc_attr_rsp[] = {
    0x35, 0x95, 0x09, 0x00, 0x00, 0x0a, 0x00, 0x01,
    0x00, 0x00, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19,
    0x12, 0x00, 0x09, 0x00, 0x04, 0x35, 0x0d, 0x35,
    0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x01, 0x35,
    0x03, 0x19, 0x00, 0x01, 0x09, 0x00, 0x05, 0x35,
    0x03, 0x19, 0x10, 0x02, 0x09, 0x00, 0x06, 0x35,
    0x09, 0x09, 0x65, 0x6e, 0x09, 0x00, 0x6a, 0x09,
    0x01, 0x00, 0x09, 0x01, 0x00, 0x25, 0x18, 0x44,
    0x65, 0x76, 0x69, 0x63, 0x65, 0x20, 0x49, 0x44,
    0x20, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65,
    0x20, 0x52, 0x65, 0x63, 0x6f, 0x72, 0x64, 0x09,
    0x01, 0x01, 0x25, 0x18, 0x44, 0x65, 0x76, 0x69,
    0x63, 0x65, 0x20, 0x49, 0x44, 0x20, 0x53, 0x65,
    0x72, 0x76, 0x69, 0x63, 0x65, 0x20, 0x52, 0x65,
    0x63, 0x6f, 0x72, 0x64, 0x09, 0x02, 0x00, 0x09,
    0x01, 0x03, 0x09, 0x02, 0x01, 0x09, 0x00, 0x06,
    0x09, 0x02, 0x02, 0x09, 0x00, 0x01, 0x09, 0x02,
    0x03, 0x09, 0x0a, 0x00, 0x09, 0x02, 0x04, 0x28,
    0x01, 0x09, 0x02, 0x05, 0x09, 0x00, 0x01
};

static uint16_t tx_tid = 0;

static void bt_sdp_cmd(uint16_t handle, uint16_t cid, uint8_t code, uint16_t tid, uint16_t len) {
    uint16_t packet_len = (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
        + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_sdp_hdr) + len);

    bt_hci_pkt_tmp.h4_hdr.type = BT_HCI_H4_TYPE_ACL;

    bt_hci_pkt_tmp.acl_hdr.handle = bt_acl_handle_pack(handle, 0x2);
    bt_hci_pkt_tmp.acl_hdr.len = packet_len - BT_HCI_H4_HDR_SIZE - BT_HCI_ACL_HDR_SIZE;

    bt_hci_pkt_tmp.l2cap_hdr.len = bt_hci_pkt_tmp.acl_hdr.len - sizeof(bt_hci_pkt_tmp.l2cap_hdr);
    bt_hci_pkt_tmp.l2cap_hdr.cid = cid;

    bt_hci_pkt_tmp.sdp_hdr.op_code = code;
    bt_hci_pkt_tmp.sdp_hdr.tid = sys_cpu_to_be16(tid);
    bt_hci_pkt_tmp.sdp_hdr.param_len = sys_cpu_to_be16(len);

    bt_host_txq_add((uint8_t *)&bt_hci_pkt_tmp, packet_len);
}

static void bt_sdp_cmd_svc_search_rsp(uint16_t handle, uint16_t cid, uint16_t tid, const uint8_t *data, uint32_t len) {
    struct bt_sdp_svc_rsp *svc_rsp = (struct bt_sdp_svc_rsp *)bt_hci_pkt_tmp.sdp_data;
    uint8_t *sdp_data = bt_hci_pkt_tmp.sdp_data + sizeof(struct bt_sdp_svc_rsp);
    uint8_t *sdp_con_state = sdp_data + len;

    svc_rsp->total_recs = sys_cpu_to_be16(0x0001);
    svc_rsp->current_recs = sys_cpu_to_be16(0x0001);
    if (data) {
        memcpy(sdp_data, data, len);
    }
    *sdp_con_state = 0;

    bt_sdp_cmd(handle, cid, BT_SDP_SVC_SEARCH_RSP, tid, sizeof(struct bt_sdp_svc_rsp) + len + 1);
}

static void bt_sdp_cmd_svc_attr_rsp(uint16_t handle, uint16_t cid, uint16_t tid, const uint8_t *data, uint32_t len) {
    struct bt_sdp_att_rsp *att_rsp = (struct bt_sdp_att_rsp *)bt_hci_pkt_tmp.sdp_data;
    uint8_t *sdp_data = bt_hci_pkt_tmp.sdp_data + sizeof(struct bt_sdp_att_rsp);
    uint8_t *sdp_con_state = sdp_data + len;

    att_rsp->att_list_len = sys_cpu_to_be16(len);
    if (data) {
        memcpy(sdp_data, data, len);
    }
    *sdp_con_state = 0;

    bt_sdp_cmd(handle, cid, BT_SDP_SVC_ATTR_RSP, tid, sizeof(struct bt_sdp_att_rsp) + len + 1);
}

static void bt_sdp_cmd_svc_search_attr_rsp(uint16_t handle, uint16_t cid, uint16_t tid, const uint8_t *data, uint32_t len) {
    struct bt_sdp_att_rsp *att_rsp = (struct bt_sdp_att_rsp *)bt_hci_pkt_tmp.sdp_data;
    uint8_t *sdp_data = bt_hci_pkt_tmp.sdp_data + sizeof(struct bt_sdp_att_rsp);
    uint8_t *sdp_con_state = sdp_data + len;

    att_rsp->att_list_len = sys_cpu_to_be16(len);
    if (data) {
        memcpy(sdp_data, data, len);
    }
    *sdp_con_state = 0;

    bt_sdp_cmd(handle, cid, BT_SDP_SVC_SEARCH_ATTR_RSP, tid, sizeof(struct bt_sdp_att_rsp) + len + 1);
}

#ifdef SDP_GET_ALL_L2CAP_ATTR
void bt_sdp_cmd_svc_search_attr_req(struct bt_dev *device, uint8_t *cont_data, uint32_t cont_len) {
    memcpy(bt_hci_pkt_tmp.sdp_data, l2cap_attr_req, sizeof(l2cap_attr_req));
    memcpy(bt_hci_pkt_tmp.sdp_data + sizeof(l2cap_attr_req), cont_data, cont_len);

    bt_sdp_cmd(device->acl_handle, device->sdp_tx_chan.dcid, BT_SDP_SVC_SEARCH_ATTR_REQ, tx_tid++, sizeof(l2cap_attr_req) + cont_len);
}
#else
static void bt_sdp_cmd_pnp_vendor_svc_search_attr_req(struct bt_dev *device) {
    memcpy(bt_hci_pkt_tmp.sdp_data, pnp_attr_req, sizeof(pnp_attr_req));
    *(bt_hci_pkt_tmp.sdp_data + sizeof(pnp_attr_req)) = 0;

    bt_sdp_cmd(device->acl_handle, device->sdp_tx_chan.dcid, BT_SDP_SVC_SEARCH_ATTR_REQ, tx_tid++, sizeof(pnp_attr_req) + 1);
}

void bt_sdp_cmd_svc_search_attr_req(struct bt_dev *device, uint8_t *cont_data, uint32_t cont_len) {
    memcpy(bt_hci_pkt_tmp.sdp_data, hid_descriptor_attr_req, sizeof(hid_descriptor_attr_req));
    memcpy(bt_hci_pkt_tmp.sdp_data + sizeof(hid_descriptor_attr_req), cont_data, cont_len);

    bt_sdp_cmd(device->acl_handle, device->sdp_tx_chan.dcid, BT_SDP_SVC_SEARCH_ATTR_REQ, tx_tid++, sizeof(hid_descriptor_attr_req) + cont_len);
}
#endif

void bt_sdp_parser(struct bt_data *bt_data) {
    const uint8_t sdp_hid_desc_list[] = {0x09, 0x02, 0x06};
    uint8_t *hid_desc = NULL;
    uint32_t hid_desc_len = 0;
    uint32_t hid_offset = 0;

    hid_desc = memmem(bt_data->base.sdp_data, bt_data->base.sdp_len, sdp_hid_desc_list, sizeof(sdp_hid_desc_list));
    hid_offset = hid_desc - bt_data->base.sdp_data;

    if (hid_desc) {
        hid_desc += 3;

        switch (*hid_desc) {
            case BT_SDP_SEQ8:
                hid_desc += 2;
                break;
            case BT_SDP_SEQ16:
                hid_desc += 3;
                break;
            case BT_SDP_SEQ32:
                hid_desc += 5;
                break;
        }

        switch (*hid_desc) {
            case BT_SDP_SEQ8:
                hid_desc += 4;
                break;
            case BT_SDP_SEQ16:
                hid_desc += 5;
                break;
            case BT_SDP_SEQ32:
                hid_desc += 7;
                break;
        }

        switch (*hid_desc++) {
            case BT_SDP_TEXT_STR8:
                hid_desc_len = *hid_desc;
                hid_desc++;
                break;
            case BT_SDP_TEXT_STR16:
                hid_desc_len = sys_be16_to_cpu(*(uint16_t *)hid_desc);
                hid_desc += 2;
                break;
            case BT_SDP_TEXT_STR32:
                hid_desc_len = sys_be32_to_cpu(*(uint32_t *)hid_desc);
                hid_desc += 4;
                break;
        }
        printf("# %s HID descriptor size: %lu Usage page: %02X%02X\n", __FUNCTION__, hid_desc_len, hid_desc[0], hid_desc[1]);
        if ((hid_offset + hid_desc_len) > bt_data->base.sdp_len) {
            printf("# %s HID descriptor size exceed buffer size: %ld, trunc\n", __FUNCTION__, bt_data->base.sdp_len);
            hid_desc_len = bt_data->base.sdp_len - hid_offset;
        }
        hid_parser(bt_data, hid_desc, hid_desc_len);
        if (bt_data->base.sdp_data) {
            free(bt_data->base.sdp_data);
            bt_data->base.sdp_data = NULL;
        }
    }
}

void bt_sdp_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt) {
    switch (bt_hci_acl_pkt->sdp_hdr.op_code) {
        case BT_SDP_SVC_SEARCH_REQ:
            bt_sdp_cmd_svc_search_rsp(device->acl_handle, device->sdp_rx_chan.dcid,
                sys_be16_to_cpu(bt_hci_acl_pkt->sdp_hdr.tid), xb1_svc_search_rsp, sizeof(xb1_svc_search_rsp));
            break;
        case BT_SDP_SVC_ATTR_REQ:
            bt_sdp_cmd_svc_attr_rsp(device->acl_handle, device->sdp_rx_chan.dcid,
                sys_be16_to_cpu(bt_hci_acl_pkt->sdp_hdr.tid), xb1_svc_attr_rsp, sizeof(xb1_svc_attr_rsp));
            break;
        case BT_SDP_SVC_SEARCH_ATTR_REQ:
            if (device->ids.type == BT_XBOX && device->ids.subtype == BT_SUBTYPE_DEFAULT) {
                /* Need to test if empty answer work for xb1 */
                bt_sdp_cmd_svc_search_attr_rsp(device->acl_handle, device->sdp_rx_chan.dcid,
                    sys_be16_to_cpu(bt_hci_acl_pkt->sdp_hdr.tid), xb1_svc_search_attr_rsp, sizeof(xb1_svc_search_attr_rsp));
            }
            else {
                bt_sdp_cmd_svc_search_attr_rsp(device->acl_handle, device->sdp_rx_chan.dcid,
                    sys_be16_to_cpu(bt_hci_acl_pkt->sdp_hdr.tid), NULL, 0);
            }
            break;
        case BT_SDP_SVC_SEARCH_ATTR_RSP:
        {
            struct bt_sdp_att_rsp *att_rsp = (struct bt_sdp_att_rsp *)bt_hci_acl_pkt->sdp_data;
            uint8_t *sdp_data = bt_hci_acl_pkt->sdp_data + sizeof(struct bt_sdp_att_rsp);
            uint8_t *sdp_con_state = sdp_data + sys_be16_to_cpu(att_rsp->att_list_len);
            uint32_t free_len = BT_SDP_DATA_SIZE - bt_adapter.data[device->ids.id].base.sdp_len;
            uint32_t cp_len = sys_be16_to_cpu(att_rsp->att_list_len);

            if (cp_len > free_len) {
                cp_len = free_len;
                printf("# %s SDP data > buffer will be trunc to %d, cp_len %ld\n", __FUNCTION__, BT_SDP_DATA_SIZE, cp_len);
            }

            switch (device->sdp_state) {
                case 0:
                    if (bt_adapter.data[device->ids.id].base.sdp_data == NULL) {
                        bt_adapter.data[device->ids.id].base.sdp_data = malloc(BT_SDP_DATA_SIZE);
                        if (bt_adapter.data[device->ids.id].base.sdp_data == NULL) {
                            printf("# dev: %ld Failed to alloc report memory\n", device->ids.id);
                            break;
                        }
                    }
                    memcpy(bt_adapter.data[device->ids.id].base.sdp_data + bt_adapter.data[device->ids.id].base.sdp_len, sdp_data, cp_len);
                    bt_adapter.data[device->ids.id].base.sdp_len += cp_len;
                    if (*sdp_con_state) {
                        bt_sdp_cmd_svc_search_attr_req(device, sdp_con_state, 1 + *sdp_con_state);
                    }
                    else {
#ifdef SDP_GET_ALL_L2CAP_ATTR
                        bt_l2cap_cmd_sdp_disconn_req(device);
                        atomic_set_bit(&device->flags, BT_DEV_SDP_DATA);
#else
                        bt_sdp_cmd_pnp_vendor_svc_search_attr_req(device);
                        device->sdp_state++;
#endif
                    }
                    break;
                case 1:
                    bt_l2cap_cmd_sdp_disconn_req(device);
                    break;
            }
            break;
        }
    }
}
