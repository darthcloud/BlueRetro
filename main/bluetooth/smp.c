/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 *
 * Based on Zephyr's subsys/bluetooth/host/smp.c:
 *   Copyright (c) 2017 Nordic Semiconductor ASA
 *   Copyright (c) 2015-2016 Intel Corporation
 */

#include <stdio.h>
#include <esp_random.h>
#include "host.h"
#include "hci.h"
#include "att_hid.h"
#include "smp.h"
#include "zephyr/smp.h"

static void bt_smp_mrand_half(struct bt_dev *device, uint8_t *data, uint32_t len);
static void bt_smp_mrand_complete(struct bt_dev *device, uint8_t *data, uint32_t len);
static void bt_smp_confirm_half(struct bt_dev *device, uint8_t *data, uint32_t len);
static void bt_smp_confirm_complete(struct bt_dev *device, uint8_t *data, uint32_t len);
static void bt_smp_stk_complete(struct bt_dev *device, uint8_t *data, uint32_t len);
static void bt_smp_key_distribution(struct bt_dev *device);
static void bt_smp_pairing_req(uint16_t handle, struct bt_smp_pairing *pairing_req);
static void bt_smp_pairing_confirm(uint16_t handle, uint8_t val[16]);
static void bt_smp_encrypt_info(uint16_t handle, uint8_t ltk[16]);
static void bt_smp_master_ident(uint16_t handle, uint8_t ediv[2], uint8_t rand[8]);
static void bt_smp_ident_info(uint16_t handle, uint8_t irk[16]);
static void bt_smp_ident_addr_info(uint16_t handle, bt_addr_le_t *le_bdaddr);
static void bt_smp_signing_info(uint16_t handle, uint8_t csrk[16]);

static void xor_128(const uint8_t p[16], const uint8_t q[16], uint8_t r[16]) {
    size_t len = 16;

    while (len--) {
        *r++ = *p++ ^ *q++;
    }
}

static int32_t smp_c1_part1(struct bt_dev *device, const uint8_t k[16], const uint8_t r[16],
          const uint8_t preq[7], const uint8_t pres[7],
          const bt_addr_le_t *ia, const bt_addr_le_t *ra,
          uint8_t enc_data[16]) {
    uint8_t p1[16];

    /* pres, preq, rat and iat are concatenated to generate p1 */
    p1[0] = ia->type;
    p1[1] = ra->type;
    memcpy(p1 + 2, preq, 7);
    memcpy(p1 + 9, pres, 7);

    /* c1 = e(k, e(k, r XOR p1) XOR p2) */

    /* Using enc_data as temporary output buffer */
    xor_128(r, p1, enc_data);

    return bt_hci_get_encrypt(device, bt_smp_confirm_half, k, enc_data);
}

static int32_t smp_c1_part2(struct bt_dev *device, const uint8_t k[16], const uint8_t r[16],
          const uint8_t preq[7], const uint8_t pres[7],
          const bt_addr_le_t *ia, const bt_addr_le_t *ra,
          uint8_t enc_data[16]) {
    uint8_t p2[16];

    /* ra is concatenated with ia and padding to generate p2 */
    memcpy(p2, ra->a.val, 6);
    memcpy(p2 + 6, ia->a.val, 6);
    (void)memset(p2 + 12, 0, 4);

    xor_128(enc_data, p2, enc_data);

    return bt_hci_get_encrypt(device, bt_smp_confirm_complete, k, enc_data);
}

static int32_t smp_s1(struct bt_dev *device, const uint8_t k[16], const uint8_t r1[16],
          const uint8_t r2[16], uint8_t out[16])
{
    /* The most significant 64-bits of r1 are discarded to generate
     * r1' and the most significant 64-bits of r2 are discarded to
     * generate r2'.
     * r1' is concatenated with r2' to generate r' which is used as
     * the 128-bit input parameter plaintextData to security function e:
     *
     *    r' = r1' || r2'
     */
    memcpy(out, r2, 8);
    memcpy(out + 8, r1, 8);

    /* s1(k, r1 , r2) = e(k, r') */
    return bt_hci_get_encrypt(device, bt_smp_stk_complete, k, out);
}

static void bt_smp_mrand_half(struct bt_dev *device, uint8_t *data, uint32_t len) {
    memcpy(device->rand, data, len);
}

static void bt_smp_mrand_complete(struct bt_dev *device, uint8_t *data, uint32_t len) {
    bt_addr_le_t le_local;
    uint8_t tk[16] = {0};

    bt_hci_get_le_local_addr(&le_local);

    memcpy(device->rand + 8, data, len);
    smp_c1_part1(device, tk, device->rand, device->preq, device->pres, &le_local, &device->le_remote_bdaddr, device->ltk);
}

static void bt_smp_confirm_half(struct bt_dev *device, uint8_t *data, uint32_t len) {
    bt_addr_le_t le_local;
    uint8_t tk[16] = {0};

    bt_hci_get_le_local_addr(&le_local);

    memcpy(device->ltk, data, len);
    smp_c1_part2(device, tk, device->rand, device->preq, device->pres, &le_local, &device->le_remote_bdaddr, device->ltk);
}

static void bt_smp_confirm_complete(struct bt_dev *device, uint8_t *data, uint32_t len) {
    memcpy(device->ltk, data, len);
    bt_smp_pairing_confirm(device->acl_handle, device->ltk);
}

static void bt_smp_stk_complete(struct bt_dev *device, uint8_t *data, uint32_t len) {
    memcpy(device->ltk, data, len);
    bt_hci_start_encryption(device->acl_handle, 0, 0, device->ltk);
}

static void bt_smp_key_distribution(struct bt_dev *device) {
    /* Unused keys IFAICT, so just use ESP random function rather than BT ctrl one */
    uint8_t tmp[16] = {0};

    if (device->ldist & BT_SMP_DIST_ENC_KEY) {
        esp_fill_random(tmp, sizeof(tmp));
        bt_smp_encrypt_info(device->acl_handle, tmp);
        esp_fill_random(tmp, sizeof(tmp));
        bt_smp_master_ident(device->acl_handle, tmp, tmp + 2);
    }
    if (device->ldist & BT_SMP_DIST_ID_KEY) {
        bt_addr_le_t le_local;
        bt_hci_get_le_local_addr(&le_local);
        esp_fill_random(tmp, sizeof(tmp));
        bt_smp_ident_info(device->acl_handle, tmp);
        bt_smp_ident_addr_info(device->acl_handle, &le_local);
    }
    if (device->ldist & BT_SMP_DIST_SIGN) {
        esp_fill_random(tmp, sizeof(tmp));
        bt_smp_signing_info(device->acl_handle, tmp);
    }

    bt_att_hid_init(device);
}

static void bt_smp_cmd(uint16_t handle, uint8_t code, uint16_t len) {
    uint16_t packet_len = (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
        + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_smp_hdr) + len);

    bt_hci_pkt_tmp.h4_hdr.type = BT_HCI_H4_TYPE_ACL;

    bt_hci_pkt_tmp.acl_hdr.handle = bt_acl_handle_pack(handle, BT_ACL_START_NO_FLUSH);
    bt_hci_pkt_tmp.acl_hdr.len = packet_len - BT_HCI_H4_HDR_SIZE - BT_HCI_ACL_HDR_SIZE;

    bt_hci_pkt_tmp.l2cap_hdr.len = bt_hci_pkt_tmp.acl_hdr.len - sizeof(bt_hci_pkt_tmp.l2cap_hdr);
    bt_hci_pkt_tmp.l2cap_hdr.cid = BT_L2CAP_CID_SMP;

    bt_hci_pkt_tmp.smp_hdr.code = code;

    bt_host_txq_add((uint8_t *)&bt_hci_pkt_tmp, packet_len);
}

static void bt_smp_pairing_req(uint16_t handle, struct bt_smp_pairing *pairing_req) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, (uint8_t *)pairing_req, sizeof(*pairing_req));

    bt_smp_cmd(handle, BT_SMP_CMD_PAIRING_REQ, sizeof(*pairing_req));
}

static void bt_smp_pairing_confirm(uint16_t handle, uint8_t val[16]) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, val, sizeof(struct bt_smp_pairing_confirm));

    bt_smp_cmd(handle, BT_SMP_CMD_PAIRING_CONFIRM, sizeof(struct bt_smp_pairing_confirm));
}

static void bt_smp_pairing_random(uint16_t handle, uint8_t val[16]) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, val, sizeof(struct bt_smp_pairing_random));

    bt_smp_cmd(handle, BT_SMP_CMD_PAIRING_RANDOM, sizeof(struct bt_smp_pairing_random));
}

static void bt_smp_encrypt_info(uint16_t handle, uint8_t ltk[16]) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, ltk, sizeof(struct bt_smp_encrypt_info));

    bt_smp_cmd(handle, BT_SMP_CMD_ENCRYPT_INFO, sizeof(struct bt_smp_pairing_random));
}

static void bt_smp_master_ident(uint16_t handle, uint8_t ediv[2], uint8_t rand[8]) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, ediv, 2);
    memcpy(bt_hci_pkt_tmp.smp_data + 2, rand, 8);

    bt_smp_cmd(handle, BT_SMP_CMD_MASTER_IDENT, sizeof(struct bt_smp_master_ident));
}

static void bt_smp_ident_info(uint16_t handle, uint8_t irk[16]) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, irk, sizeof(struct bt_smp_ident_info));

    bt_smp_cmd(handle, BT_SMP_CMD_IDENT_INFO, sizeof(struct bt_smp_ident_info));
}

static void bt_smp_ident_addr_info(uint16_t handle, bt_addr_le_t *le_bdaddr) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, le_bdaddr, sizeof(struct bt_smp_ident_addr_info));

    bt_smp_cmd(handle, BT_SMP_CMD_IDENT_ADDR_INFO, sizeof(struct bt_smp_ident_addr_info));
}

static void bt_smp_signing_info(uint16_t handle, uint8_t csrk[16]) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, csrk, sizeof(struct bt_smp_signing_info));

    bt_smp_cmd(handle, BT_SMP_CMD_SIGNING_INFO, sizeof(struct bt_smp_signing_info));
}

void bt_smp_pairing_start(struct bt_dev *device) {
    struct bt_smp_pairing pairing_req = {
        .io_capability = BT_SMP_IO_NO_INPUT_OUTPUT,
        .oob_flag = BT_SMP_OOB_NOT_PRESENT,
        .auth_req = BT_SMP_AUTH_BONDING,
        .max_key_size = BT_SMP_MAX_ENC_KEY_SIZE,
        .init_key_dist = BT_SMP_DIST_ID_KEY,
        .resp_key_dist = BT_SMP_DIST_ENC_KEY,
    };
    device->preq[0] = BT_SMP_CMD_PAIRING_REQ;
    memcpy(&device->preq[1], (uint8_t *)&pairing_req, sizeof(pairing_req));
    bt_smp_pairing_req(device->acl_handle, &pairing_req);
}

void bt_smp_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    switch (bt_hci_acl_pkt->smp_hdr.code) {
        case BT_SMP_CMD_PAIRING_RSP:
        {
            struct bt_smp_pairing *pairing_rsp = (struct bt_smp_pairing *)bt_hci_acl_pkt->smp_data;
            printf("# BT_SMP_CMD_PAIRING_RSP\n");

            device->ldist = pairing_rsp->init_key_dist;
            device->rdist = pairing_rsp->resp_key_dist;
            device->pres[0] = BT_SMP_CMD_PAIRING_RSP;
            memcpy(&device->pres[1], (uint8_t *)pairing_rsp, sizeof(*pairing_rsp));

            bt_hci_get_random(device, bt_smp_mrand_half);
            bt_hci_get_random(device, bt_smp_mrand_complete);
            break;
        }
        case BT_SMP_CMD_PAIRING_CONFIRM:
        {
            //struct bt_smp_pairing_confirm *pairing_confirm = (struct bt_smp_pairing_confirm *)bt_hci_acl_pkt->smp_data;
            printf("# BT_SMP_CMD_PAIRING_CONFIRM\n");

            bt_smp_pairing_random(device->acl_handle, device->rand);
            break;
        }
        case BT_SMP_CMD_PAIRING_RANDOM:
        {
            uint8_t tk[16] = {0};
            struct bt_smp_pairing_random *pairing_random = (struct bt_smp_pairing_random *)bt_hci_acl_pkt->smp_data;
            printf("# BT_SMP_CMD_PAIRING_RANDOM\n");

            /* We should validate remote confirm here, but we are lazy */

            memcpy(device->rrand, pairing_random->val, sizeof(device->rrand));
            smp_s1(device, tk, device->rrand, device->rand, device->ltk);

            if (!device->rdist) {
                bt_smp_key_distribution(device);
            }
            break;
        }
        case BT_SMP_CMD_PAIRING_FAIL:
        {
            struct bt_smp_pairing_fail *pairing_fail = (struct bt_smp_pairing_fail *)bt_hci_acl_pkt->smp_data;
            printf("# BT_SMP_CMD_PAIRING_FAIL reason: %02X\n", pairing_fail->reason);
            break;
        }
        case BT_SMP_CMD_ENCRYPT_INFO:
        {
            struct bt_smp_encrypt_info *encrypt_info = (struct bt_smp_encrypt_info *)bt_hci_acl_pkt->smp_data;
            printf("# BT_SMP_CMD_ENCRYPT_INFO\n");

            bt_host_store_le_ltk(&device->le_remote_bdaddr, encrypt_info);
            break;
        }
        case BT_SMP_CMD_MASTER_IDENT:
        {
            struct bt_smp_master_ident *master_ident = (struct bt_smp_master_ident *)bt_hci_acl_pkt->smp_data;
            printf("# BT_SMP_CMD_MASTER_IDENT\n");

            device->rdist &= ~BT_SMP_DIST_ENC_KEY;

            bt_host_store_le_ident(&device->le_remote_bdaddr, master_ident);

            if (!device->rdist) {
                bt_smp_key_distribution(device);
            }
            
            bt_hci_add_to_accept_list(&device->le_remote_bdaddr);
            break;
        }
        case BT_SMP_CMD_IDENT_INFO:
            printf("# BT_SMP_CMD_IDENT_INFO\n");
            break;
        case BT_SMP_CMD_IDENT_ADDR_INFO:
            printf("# BT_SMP_CMD_IDENT_ADDR_INFO\n");

            device->rdist &= ~BT_SMP_DIST_ID_KEY;

            if (!device->rdist) {
                bt_smp_key_distribution(device);
            }
            break;
        case BT_SMP_CMD_SIGNING_INFO:
            printf("# BT_SMP_CMD_SIGNING_INFO\n");

            device->rdist &= ~BT_SMP_DIST_SIGN;

            if (!device->rdist) {
                bt_smp_key_distribution(device);
            }
            break;
        default:
            printf("# Unsupported OPCODE: 0x%02X\n", bt_hci_acl_pkt->smp_hdr.code);
    }
}
