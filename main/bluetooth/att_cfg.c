/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <dirent.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "host.h"
#include "hci.h"
#include "att.h"
#include "att_cfg.h"
#include "zephyr/uuid.h"
#include "zephyr/att.h"
#include "zephyr/gatt.h"
#include "adapter/config.h"
#include "adapter/memory_card.h"
#include "adapter/gameid.h"
#include "system/manager.h"
#include "system/fs.h"

#define ATT_MAX_LEN 512
#define BR_ABI_VER 2

#define CFG_CMD_GET_ABI_VER 0x01
#define CFG_CMD_GET_FW_VER 0x02
#define CFG_CMD_GET_BDADDR 0x03
#define CFG_CMD_GET_GAMEID 0x04
#define CFG_CMD_GET_CFG_SRC 0x05
#define CFG_CMD_GET_FILE 0x06
#define CFG_CMD_GET_FW_NAME 0x07
#define CFG_CMD_SET_DEFAULT_CFG 0x10
#define CFG_CMD_SET_GAMEID_CFG 0x11
#define CFG_CMD_OPEN_DIR 0x12
#define CFG_CMD_CLOSE_DIR 0x13
#define CFG_CMD_DEL_FILE 0x14
#define CFG_CMD_SYS_DEEP_SLEEP 0x37
#define CFG_CMD_SYS_RESET 0x38
#define CFG_CMD_SYS_FACTORY 0x39
#define CFG_CMD_OTA_END 0x5A
#define CFG_CMD_OTA_START 0xA5
#define CFG_CMD_OTA_ABORT 0xDE

enum {
    GATT_GRP_HDL = 0x0001,
    GATT_SRVC_CH_ATT_HDL,
    GATT_SRVC_CH_CHRC_HDL,
    GAP_GRP_HDL = 0x0014,
    GAP_DEV_NAME_ATT_HDL,
    GAP_DEV_NAME_CHRC_HDL,
    GAP_APP_ATT_HDL,
    GAP_APP_CHRC_HDL,
    BR_GRP_HDL = 0x0040,
    BR_GLBL_CFG_ATT_HDL,
    BR_GLBL_CFG_CHRC_HDL,
    BR_OUT_CFG_CTRL_ATT_HDL,
    BR_OUT_CFG_CTRL_CHRC_HDL,
    BR_OUT_CFG_DATA_ATT_HDL,
    BR_OUT_CFG_DATA_CHRC_HDL,
    BR_IN_CFG_CTRL_ATT_HDL,
    BR_IN_CFG_CTRL_CHRC_HDL,
    BR_IN_CFG_DATA_ATT_HDL,
    BR_IN_CFG_DATA_CHRC_HDL,
    BR_ABI_VER_ATT_HDL,
    BR_ABI_VER_CHRC_HDL, /* Deprecated, use BR_CFG_CMD_CHRC_HDL */
    BR_CFG_CMD_ATT_HDL,
    BR_CFG_CMD_CHRC_HDL,
    BR_OTA_FW_DATA_ATT_HDL,
    BR_OTA_FW_DATA_CHRC_HDL,
    BR_FW_VER_ATT_HDL,
    BR_FW_VER_CHRC_HDL, /* Deprecated, use BR_CFG_CMD_CHRC_HDL */
    BR_MC_CTRL_ATT_HDL,
    BR_MC_CTRL_CHRC_HDL,
    BR_MC_DATA_ATT_HDL,
    BR_MC_DATA_CHRC_HDL,
    BR_BD_ADDR_ATT_HDL,
    BR_BD_ADDR_CHRC_HDL, /* Deprecated, use BR_CFG_CMD_CHRC_HDL */
    LAST_HDL = BR_BD_ADDR_CHRC_HDL,
    MAX_HDL,
};

static uint8_t br_grp_base_uuid[] = {0x00, 0x9a, 0x79, 0x76, 0xa1, 0x2f, 0x4b, 0x31, 0xb0, 0xfa, 0x80, 0x51, 0x56, 0x0f, 0x83, 0x56};

static uint16_t mtu = 23;
static esp_ota_handle_t ota_hdl = 0;
static const esp_partition_t *update_partition = NULL;
static uint16_t out_cfg_id = 0;
static uint16_t in_cfg_offset = 0;
static uint16_t in_cfg_id = 0;
static uint32_t mc_offset = 0;
static uint8_t cfg_cmd = 0;
static DIR *d = NULL;
static struct dirent *dir = NULL;

static void bt_att_cmd_gatt_char_read_type_rsp(uint16_t handle) {
    struct bt_att_read_type_rsp *rd_type_rsp = (struct bt_att_read_type_rsp *)bt_hci_pkt_tmp.att_data;
    uint8_t *data = rd_type_rsp->data->value;

    printf("# %s\n", __FUNCTION__);

    rd_type_rsp->len = 7;

    rd_type_rsp->data->handle = GATT_SRVC_CH_ATT_HDL;
    *data = BT_GATT_CHRC_INDICATE;
    data++;
    *(uint16_t *)data = GATT_SRVC_CH_CHRC_HDL;
    data += 2;
    *(uint16_t *)data = BT_UUID_GATT_SC;

    bt_att_cmd(handle, BT_ATT_OP_READ_TYPE_RSP, sizeof(rd_type_rsp->len) + rd_type_rsp->len);
}

static void bt_att_cmd_gap_char_read_type_rsp(uint16_t handle) {
    struct bt_att_read_type_rsp *rd_type_rsp = (struct bt_att_read_type_rsp *)bt_hci_pkt_tmp.att_data;
    struct bt_att_data *name_data = (struct bt_att_data *)((uint8_t *)rd_type_rsp->data + 0);
    struct bt_att_data *app_data = (struct bt_att_data *)((uint8_t *)rd_type_rsp->data + 7);
    uint8_t *data;

    printf("# %s\n", __FUNCTION__);

    rd_type_rsp->len = 7;

    name_data->handle = GAP_DEV_NAME_ATT_HDL;
    data = name_data->value;
    *data = BT_GATT_CHRC_READ;
    data++;
    *(uint16_t *)data = GAP_DEV_NAME_CHRC_HDL;
    data += 2;
    *(uint16_t *)data = BT_UUID_GAP_DEVICE_NAME;

    app_data->handle = GAP_APP_ATT_HDL;
    data = app_data->value;
    *data = BT_GATT_CHRC_READ;
    data++;
    *(uint16_t *)data = GAP_APP_CHRC_HDL;
    data += 2;
    *(uint16_t *)data = BT_UUID_GAP_APPEARANCE;

    bt_att_cmd(handle, BT_ATT_OP_READ_TYPE_RSP, sizeof(rd_type_rsp->len) + rd_type_rsp->len * 2);
}

static void bt_att_cmd_blueretro_char_read_type_rsp(uint16_t handle, uint16_t start) {
    struct bt_att_read_type_rsp *rd_type_rsp = (struct bt_att_read_type_rsp *)bt_hci_pkt_tmp.att_data;
    uint8_t *data = rd_type_rsp->data->value;

    printf("# %s\n", __FUNCTION__);

    rd_type_rsp->len = 21;

    if (!(start & 0x01)) {
        rd_type_rsp->data->handle = start + 1;
    }
    else {
        rd_type_rsp->data->handle = start;
    }

    switch (rd_type_rsp->data->handle + 1) {
        case BR_ABI_VER_CHRC_HDL:
        case BR_FW_VER_CHRC_HDL:
        case BR_BD_ADDR_CHRC_HDL:
            *data++ = BT_GATT_CHRC_READ;
            break;
        case BR_OUT_CFG_CTRL_CHRC_HDL:
        case BR_IN_CFG_CTRL_CHRC_HDL:
        case BR_OTA_FW_DATA_CHRC_HDL:
        case BR_MC_CTRL_CHRC_HDL:
            *data++ = BT_GATT_CHRC_WRITE;
            break;
        default:
            *data++ = BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE;
            break;
    }

    *(uint16_t *)data = rd_type_rsp->data->handle + 1;
    data += 2;
    memcpy(data, br_grp_base_uuid, sizeof(br_grp_base_uuid));
    *data = (rd_type_rsp->data->handle - BR_GLBL_CFG_ATT_HDL)/2 + 1;

    bt_att_cmd(handle, BT_ATT_OP_READ_TYPE_RSP, sizeof(rd_type_rsp->len) + rd_type_rsp->len);
}

static void bt_att_cmd_dev_name_rd_rsp(uint16_t handle) {
    const char *str = bt_hci_get_device_name();
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.att_data, str, strlen(str));

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, strlen(str));
}

static void bt_att_cmd_app_rd_rsp(uint16_t handle) {
    printf("# %s\n", __FUNCTION__);

    *(uint16_t *)bt_hci_pkt_tmp.att_data = 964; /* HID Gamepad */

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, sizeof(uint16_t));
}

static void bt_att_cmd_global_cfg_rd_rsp(uint16_t handle, uint16_t offset) {
    uint32_t len = 0;
    printf("# %s\n", __FUNCTION__);

    len = sizeof(config.global_cfg);
    memcpy(bt_hci_pkt_tmp.att_data, (void *)&config.global_cfg, len);

    bt_att_cmd(handle, offset ? BT_ATT_OP_READ_BLOB_RSP : BT_ATT_OP_READ_RSP, len);
}

static void bt_att_cmd_out_cfg_rd_rsp(uint16_t handle, uint16_t offset) {
    uint32_t len = 0;
    printf("# %s\n", __FUNCTION__);

    if (offset > sizeof(config.out_cfg[0])) {
        len = 0;
    }
    else {
        len = sizeof(config.out_cfg[0]) - offset;

        if (len > (mtu - 1)) {
            len = mtu - 1;
        }

        memcpy(bt_hci_pkt_tmp.att_data, (void *)&config.out_cfg[out_cfg_id] + offset, len);
    }

    bt_att_cmd(handle, offset ? BT_ATT_OP_READ_BLOB_RSP : BT_ATT_OP_READ_RSP, len);
}

static void bt_att_cmd_in_cfg_rd_rsp(uint16_t handle, uint16_t offset) {
    uint32_t len = 0;
    uint32_t cfg_len = (sizeof(config.in_cfg[0]) - (ADAPTER_MAPPING_MAX * sizeof(config.in_cfg[0].map_cfg[0]) - config.in_cfg[in_cfg_id].map_size * sizeof(config.in_cfg[0].map_cfg[0])));
    uint32_t sum_offset = in_cfg_offset + offset;

    printf("# %s\n", __FUNCTION__);

    if (sum_offset > cfg_len || offset > ATT_MAX_LEN) {
        len = 0;
    }
    else {
        len = cfg_len - sum_offset;

        if (len > (mtu - 1)) {
            len = mtu - 1;
        }

        if ((offset + len) > ATT_MAX_LEN) {
            len = ATT_MAX_LEN - offset;
        }

        memcpy(bt_hci_pkt_tmp.att_data, (void *)&config.in_cfg[in_cfg_id] + sum_offset, len);
    }

    bt_att_cmd(handle, offset ? BT_ATT_OP_READ_BLOB_RSP : BT_ATT_OP_READ_RSP, len);
}

static void bt_att_cmd_mc_rd_rsp(uint16_t handle, uint8_t blob) {
    uint32_t len = 0;
    uint32_t block = mc_offset / 4096 + 1;
    printf("# %s\n", __FUNCTION__);

    len = (block * 4096) - mc_offset;

    if (len > (mtu - 2)) {
        len = mtu - 2;
        len &= 0xFFFFFFFC;
    }

    mc_read(mc_offset, bt_hci_pkt_tmp.att_data, len);

    bt_att_cmd(handle, (blob) ? BT_ATT_OP_READ_BLOB_RSP : BT_ATT_OP_READ_RSP, len);

    mc_offset += len;
}

static void bt_att_cfg_cmd_abi_ver_rsp(uint16_t handle) {
    printf("# ABI version: %d\n", BR_ABI_VER);

    bt_hci_pkt_tmp.att_data[0] = BR_ABI_VER;

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, 1);
}

static void bt_att_cfg_cmd_fw_ver_rsp(uint16_t handle) {
    const esp_app_desc_t *app_desc = esp_app_get_description();

    memcpy(bt_hci_pkt_tmp.att_data, app_desc->version, 23);
    bt_hci_pkt_tmp.att_data[23] = 0;
    printf("# %s %s\n", __FUNCTION__, bt_hci_pkt_tmp.att_data);

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, 23);
}

static void bt_att_cfg_cmd_fw_name_rsp(uint16_t handle) {
    const esp_app_desc_t *app_desc = esp_app_get_description();

    memcpy(bt_hci_pkt_tmp.att_data, app_desc->project_name, 23);
    bt_hci_pkt_tmp.att_data[23] = 0;
    printf("# %s %s\n", __FUNCTION__, bt_hci_pkt_tmp.att_data);

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, 23);
}

static void bt_att_cfg_cmd_bdaddr_rsp(uint16_t handle) {
    bt_addr_le_t bdaddr;

    bt_hci_get_le_local_addr(&bdaddr);

    memcpy(bt_hci_pkt_tmp.att_data, bdaddr.a.val, sizeof(bdaddr.a.val));

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, 6);
}

static void bt_att_cfg_cmd_gameid_rsp(uint16_t handle) {
    const char *gameid = gid_get();
    uint32_t len = strlen(gameid);

    if (len > 23) {
        len = 23;
    }

    memcpy(bt_hci_pkt_tmp.att_data, gameid, len);

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, len);
}

static void bt_att_cfg_cmd_cfg_src_rsp(uint16_t handle) {
    bt_hci_pkt_tmp.att_data[0] = config_get_src();
    printf("# %s %s\n", __FUNCTION__, bt_hci_pkt_tmp.att_data);

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, 1);
}

static void bt_att_cfg_cmd_file_rsp(uint16_t handle) {
    uint32_t len;

    dir = readdir(d);

    if (dir == NULL) {
        len = 0;
    }
    else {
        len = strlen(dir->d_name);
    }

    if (len > 23) {
        len = 23;
    }

    memcpy(bt_hci_pkt_tmp.att_data, dir->d_name, len);

    bt_att_cmd(handle, BT_ATT_OP_READ_RSP, len);
}

static void bt_att_cmd_read_group_rsp(uint16_t handle, uint16_t start, uint16_t end) {
    struct bt_att_read_group_rsp *rd_grp_rsp = (struct bt_att_read_group_rsp *)bt_hci_pkt_tmp.att_data;
    struct bt_att_group_data *gatt_data = (struct bt_att_group_data *)((uint8_t *)rd_grp_rsp->data + 0);
    struct bt_att_group_data *gap_data = (struct bt_att_group_data *)((uint8_t *)rd_grp_rsp->data + 6);
    uint32_t len = sizeof(*rd_grp_rsp) - sizeof(rd_grp_rsp->data);

    printf("# %s\n", __FUNCTION__);

    if (start <= GAP_APP_CHRC_HDL) {
        rd_grp_rsp->len = 6;

        if (start <= GATT_GRP_HDL && end >= GATT_SRVC_CH_CHRC_HDL) {
            gatt_data->start_handle = GATT_GRP_HDL;
            gatt_data->end_handle = GATT_SRVC_CH_CHRC_HDL;
            *(uint16_t *)gatt_data->value = BT_UUID_GATT;
            len += rd_grp_rsp->len;
        }

        if (start <= GAP_GRP_HDL && end >= GAP_APP_CHRC_HDL) {
            gap_data->start_handle = GAP_GRP_HDL;
            gap_data->end_handle = GAP_APP_CHRC_HDL;
            *(uint16_t *)gap_data->value = BT_UUID_GAP;
            len += rd_grp_rsp->len;
        }
    }
    else {
        rd_grp_rsp->len = 20;

        if (start <= BR_GRP_HDL && end >= LAST_HDL) {
            gatt_data->start_handle = BR_GRP_HDL;
            gatt_data->end_handle = LAST_HDL;
            memcpy(gatt_data->value, br_grp_base_uuid, sizeof(br_grp_base_uuid));
            len += rd_grp_rsp->len;
        }
    }

    bt_att_cmd(handle, BT_ATT_OP_READ_GROUP_RSP, len);
}

static void bt_att_cfg_cmd_rd_hdlr(uint16_t handle) {
    switch (cfg_cmd) {
        case CFG_CMD_GET_ABI_VER:
            bt_att_cfg_cmd_abi_ver_rsp(handle);
            break;
        case CFG_CMD_GET_FW_VER:
            bt_att_cfg_cmd_fw_ver_rsp(handle);
            break;
        case CFG_CMD_GET_BDADDR:
            bt_att_cfg_cmd_bdaddr_rsp(handle);
            break;
        case CFG_CMD_GET_GAMEID:
            bt_att_cfg_cmd_gameid_rsp(handle);
            break;
        case CFG_CMD_GET_CFG_SRC:
            bt_att_cfg_cmd_cfg_src_rsp(handle);
            break;
        case CFG_CMD_GET_FILE:
            bt_att_cfg_cmd_file_rsp(handle);
            break;
        case CFG_CMD_GET_FW_NAME:
            bt_att_cfg_cmd_fw_name_rsp(handle);
            break;
        default:
            printf("# Invalid read cfg cmd: %02X\n", cfg_cmd);
            bt_att_cfg_cmd_abi_ver_rsp(handle);
            break;
    }
}

static void bt_att_cfg_cmd_wr_hdlr(struct bt_dev *device, uint8_t *data, uint32_t len) {
    cfg_cmd = data[0];

    switch (cfg_cmd) {
        case CFG_CMD_SET_DEFAULT_CFG:
        {
            char tmp_str[32] = "/fs/";
            config_init(DEFAULT_CFG);
            strcat(tmp_str, gid_get());
            remove(tmp_str);
            break;
        }
        case CFG_CMD_SET_GAMEID_CFG:
            config_update(GAMEID_CFG);
            break;
        case CFG_CMD_OTA_START:
            update_partition = esp_ota_get_next_update_partition(NULL);
            if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_hdl) == 0) {
                printf("# OTA FW Update started...\n");
            }
            else {
                esp_ota_abort(ota_hdl);
                ota_hdl = 0;
                printf("# OTA FW Update start fail\n");
            }
            break;
        case CFG_CMD_OTA_END:
            if (esp_ota_end(ota_hdl) == 0) {
                if (esp_ota_set_boot_partition(update_partition) == 0) {
                    ota_hdl = 0;
                    sys_mgr_cmd(SYS_MGR_CMD_ADAPTER_RST);
                    printf("# OTA FW Update sucessfull! Restarting...\n");
                }
                else {
                    printf("# OTA FW Update set partition fail\n");
                }
            }
            else {
                printf("# OTA FW Update end fail\n");
            }
            break;
        case CFG_CMD_SYS_DEEP_SLEEP:
            printf("# ESP32 going in deep sleep\n");
            sys_mgr_cmd(SYS_MGR_CMD_DEEP_SLEEP);
            break;
        case CFG_CMD_SYS_RESET:
        {
            printf("# Reset ESP32\n");
            sys_mgr_cmd(SYS_MGR_CMD_ADAPTER_RST);
            break;
        }
        case CFG_CMD_SYS_FACTORY:
        {
            sys_mgr_cmd(SYS_MGR_CMD_FACTORY_RST);
            break;
        }
        case CFG_CMD_OTA_ABORT:
            if (ota_hdl) {
                esp_ota_abort(ota_hdl);
                ota_hdl = 0;
                printf("# OTA FW Update abort from WebUI\n");
            }
            break;
        case CFG_CMD_OPEN_DIR:
            if (d) {
                closedir(d);
            }
            d = opendir(ROOT);
            break;
        case CFG_CMD_CLOSE_DIR:
            if (d) {
                closedir(d);
            }
            break;
        case CFG_CMD_DEL_FILE:
        {
            char tmp_str[32] = "/fs/";
            memcpy(&tmp_str[4], &data[1], len - 1);
            tmp_str[4 + len - 1] = 0;
            printf("# delete file: %s\n", tmp_str);
            if (strncmp(&tmp_str[4], gid_get(), len) == 0) {
                printf("# Deleting current cfg, switch to default\n");
                config_init(DEFAULT_CFG);
            }
            remove(tmp_str);
            break;
        }
        default:
            break;
    }
    bt_att_cmd_wr_rsp(device->acl_handle);
}

void bt_att_cfg_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    switch (bt_hci_acl_pkt->att_hdr.code) {
        case BT_ATT_OP_MTU_REQ:
        {
            struct bt_att_exchange_mtu_req *mtu_req = (struct bt_att_exchange_mtu_req *)bt_hci_acl_pkt->att_data;
            uint16_t max_mtu = bt_att_get_le_max_mtu();
            printf("# BT_ATT_OP_MTU_REQ\n");
            if (mtu_req->mtu < max_mtu) {
                mtu = mtu_req->mtu;
            }
            else {
                mtu = max_mtu;
            }
            printf("# Set mtu %d\n", mtu);
            bt_att_cmd_mtu_rsp(device->acl_handle, mtu);
            break;
        }
        case BT_ATT_OP_FIND_INFO_REQ:
        {
            struct bt_att_find_info_req *find_info_req = (struct bt_att_find_info_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_FIND_INFO_REQ %04X %04X\n", find_info_req->start_handle, find_info_req->end_handle);
            bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_FIND_INFO_REQ, find_info_req->start_handle, BT_ATT_ERR_NOT_SUPPORTED);
            break;
        }
        case BT_ATT_OP_FIND_TYPE_REQ:
        {
            struct bt_att_find_type_req *fd_type_req = (struct bt_att_find_type_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_FIND_TYPE_REQ %04X %04X\n", fd_type_req->start_handle, fd_type_req->end_handle);
            bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_FIND_TYPE_REQ, fd_type_req->start_handle, BT_ATT_ERR_NOT_SUPPORTED);
            break;
        }
        case BT_ATT_OP_READ_TYPE_REQ:
        {
            struct bt_att_read_type_req *rd_type_req = (struct bt_att_read_type_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_READ_TYPE_REQ\n");
            uint16_t start = rd_type_req->start_handle;
            uint16_t end = rd_type_req->end_handle;

            if (*(uint16_t *)rd_type_req->uuid == BT_UUID_GATT_CHRC) {
                /* GATT */
                if (start >= GATT_GRP_HDL && start < GATT_SRVC_CH_CHRC_HDL && end >= GATT_SRVC_CH_CHRC_HDL) {
                    bt_att_cmd_gatt_char_read_type_rsp(device->acl_handle);
                }
                /* GAP */
                else if (start >= GATT_SRVC_CH_CHRC_HDL && start < GAP_APP_CHRC_HDL && end >= GAP_APP_CHRC_HDL) {
                    bt_att_cmd_gap_char_read_type_rsp(device->acl_handle);
                }
                /* BLUERETRO */
                else if (start >= GAP_APP_CHRC_HDL && start < LAST_HDL && end >= LAST_HDL) {
                    bt_att_cmd_blueretro_char_read_type_rsp(device->acl_handle, start);
                }
                else {
                    bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_READ_TYPE_REQ, rd_type_req->start_handle, BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
                }
            }
            else {
                bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_READ_TYPE_REQ, rd_type_req->start_handle, BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
            }
            break;
        }
        case BT_ATT_OP_READ_REQ:
        {
            struct bt_att_read_req *rd_req = (struct bt_att_read_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_READ_REQ\n");

            switch (rd_req->handle) {
                case GAP_DEV_NAME_CHRC_HDL:
                    bt_att_cmd_dev_name_rd_rsp(device->acl_handle);
                    break;
                case GAP_APP_CHRC_HDL:
                    bt_att_cmd_app_rd_rsp(device->acl_handle);
                    break;
                case BR_GLBL_CFG_CHRC_HDL:
                    bt_att_cmd_global_cfg_rd_rsp(device->acl_handle, 0);
                    break;
                case BR_OUT_CFG_DATA_CHRC_HDL:
                    bt_att_cmd_out_cfg_rd_rsp(device->acl_handle, 0);
                    break;
                case BR_IN_CFG_DATA_CHRC_HDL:
                    bt_att_cmd_in_cfg_rd_rsp(device->acl_handle, 0);
                    break;
                case BR_ABI_VER_CHRC_HDL:
                    bt_att_cfg_cmd_abi_ver_rsp(device->acl_handle);
                    break;
                case BR_FW_VER_CHRC_HDL:
                    bt_att_cfg_cmd_fw_ver_rsp(device->acl_handle);
                    break;
                case BR_MC_DATA_CHRC_HDL:
                    bt_att_cmd_mc_rd_rsp(device->acl_handle, 0);
                    break;
                case BR_BD_ADDR_CHRC_HDL:
                    bt_att_cfg_cmd_bdaddr_rsp(device->acl_handle);
                    break;
                case BR_CFG_CMD_CHRC_HDL:
                    bt_att_cfg_cmd_rd_hdlr(device->acl_handle);
                    break;
                default:
                    bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_READ_REQ, rd_req->handle, BT_ATT_ERR_INVALID_HANDLE);
                    break;
            }
            break;
        }
        case BT_ATT_OP_READ_BLOB_REQ:
        {
            struct bt_att_read_blob_req *rd_blob_req = (struct bt_att_read_blob_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_READ_BLOB_RSP\n");

            switch (rd_blob_req->handle) {
                case BR_GLBL_CFG_CHRC_HDL:
                    bt_att_cmd_global_cfg_rd_rsp(device->acl_handle, rd_blob_req->offset);
                    break;
                case BR_OUT_CFG_DATA_CHRC_HDL:
                    bt_att_cmd_out_cfg_rd_rsp(device->acl_handle, rd_blob_req->offset);
                    break;
                case BR_IN_CFG_DATA_CHRC_HDL:
                    bt_att_cmd_in_cfg_rd_rsp(device->acl_handle, rd_blob_req->offset);
                    break;
                case BR_MC_DATA_CHRC_HDL:
                    bt_att_cmd_mc_rd_rsp(device->acl_handle, 1);
                    break;
                default:
                    bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_READ_BLOB_REQ, rd_blob_req->handle, BT_ATT_ERR_INVALID_HANDLE);
                    break;
            }
            break;
        }
        case BT_ATT_OP_READ_GROUP_REQ:
        {
            struct bt_att_read_group_req *rd_grp_req = (struct bt_att_read_group_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_READ_GROUP_REQ\n");
            if (rd_grp_req->start_handle < MAX_HDL && *(uint16_t *)rd_grp_req->uuid == BT_UUID_GATT_PRIMARY) {
                bt_att_cmd_read_group_rsp(device->acl_handle, rd_grp_req->start_handle, rd_grp_req->end_handle);
            }
            else {
                bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_READ_GROUP_REQ, rd_grp_req->start_handle, BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
            }
            break;
        }
        case BT_ATT_OP_WRITE_REQ:
        {
            struct bt_att_write_req *wr_req = (struct bt_att_write_req *)bt_hci_acl_pkt->att_data;
            uint16_t *data = (uint16_t *)wr_req->value;
            uint32_t att_len = len - (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_att_hdr));
            uint32_t data_len = att_len - sizeof(wr_req->handle);
            printf("# BT_ATT_OP_WRITE_REQ len: %ld\n", data_len);
            switch (wr_req->handle) {
                case BR_GLBL_CFG_CHRC_HDL:
                    printf("# BR_GLBL_CFG_CHRC_HDL %04X\n", wr_req->handle);
                    memcpy((void *)&config.global_cfg, wr_req->value, sizeof(config.global_cfg));
                    config_update(config_get_src());
                    bt_att_cmd_wr_rsp(device->acl_handle);
                    break;
                case BR_OUT_CFG_DATA_CHRC_HDL:
                    memcpy((void *)&config.out_cfg[out_cfg_id], wr_req->value, sizeof(config.out_cfg[0]));
                    config_update(config_get_src());
                    bt_att_cmd_wr_rsp(device->acl_handle);
                    break;
                case BR_IN_CFG_DATA_CHRC_HDL:
                    memcpy((void *)&config.in_cfg[in_cfg_id] + in_cfg_offset, wr_req->value, data_len);
                    config_update(config_get_src());
                    bt_att_cmd_wr_rsp(device->acl_handle);
                    break;
                case BR_OUT_CFG_CTRL_CHRC_HDL:
                    out_cfg_id = *data;
                    bt_att_cmd_wr_rsp(device->acl_handle);
                    break;
                case BR_IN_CFG_CTRL_CHRC_HDL:
                    in_cfg_id = *data++;
                    in_cfg_offset = *data;
                    bt_att_cmd_wr_rsp(device->acl_handle);
                    break;
                case BR_CFG_CMD_CHRC_HDL:
                    bt_att_cfg_cmd_wr_hdlr(device, wr_req->value, data_len);
                    break;
                case BR_OTA_FW_DATA_CHRC_HDL:
                    esp_ota_write(ota_hdl, wr_req->value, data_len);
                    bt_att_cmd_wr_rsp(device->acl_handle);
                    break;
                case BR_MC_CTRL_CHRC_HDL:
                    mc_offset = *(uint32_t *)wr_req->value;
                    bt_att_cmd_wr_rsp(device->acl_handle);
                    break;
                case BR_MC_DATA_CHRC_HDL:
                    mc_write(mc_offset, wr_req->value, data_len);
                    mc_offset += data_len;
                    bt_att_cmd_wr_rsp(device->acl_handle);
                    break;
                default:
                    bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_WRITE_REQ, wr_req->handle, BT_ATT_ERR_INVALID_HANDLE);
                    break;
            }
            break;
        }
        case BT_ATT_OP_PREPARE_WRITE_REQ:
        {
            struct bt_att_prepare_write_req *prep_wr_req = (struct bt_att_prepare_write_req *)bt_hci_acl_pkt->att_data;
            uint32_t att_len = len - (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_att_hdr));
            uint32_t data_len = att_len - (sizeof(prep_wr_req->handle) + sizeof(prep_wr_req->offset));
            printf("# BT_ATT_OP_PREPARE_WRITE_REQ len: %ld offset: %d\n", data_len, prep_wr_req->offset);
            switch (prep_wr_req->handle) {
                case BR_IN_CFG_DATA_CHRC_HDL:
                    memcpy((void *)&config.in_cfg[in_cfg_id] + in_cfg_offset + prep_wr_req->offset, prep_wr_req->value, data_len);
                    bt_att_cmd_prep_wr_rsp(device->acl_handle, bt_hci_acl_pkt->att_data, att_len);
                    break;
                case BR_OTA_FW_DATA_CHRC_HDL:
                    esp_ota_write(ota_hdl, prep_wr_req->value, data_len);
                    bt_att_cmd_prep_wr_rsp(device->acl_handle, bt_hci_acl_pkt->att_data, att_len);
                    break;
                case BR_MC_DATA_CHRC_HDL:
                    mc_write(mc_offset, prep_wr_req->value, data_len);
                    mc_offset += data_len;
                    bt_att_cmd_prep_wr_rsp(device->acl_handle, bt_hci_acl_pkt->att_data, att_len);
                    break;
                default:
                    bt_att_cmd_error_rsp(device->acl_handle, BT_ATT_OP_PREPARE_WRITE_REQ, prep_wr_req->handle, BT_ATT_ERR_INVALID_HANDLE);
                    break;
            }
            break;
        }
        case BT_ATT_OP_EXEC_WRITE_REQ:
        {
            struct bt_att_exec_write_req *exec_wr_req = (struct bt_att_exec_write_req *)bt_hci_acl_pkt->att_data;
            printf("# BT_ATT_OP_EXEC_WRITE_REQ\n");
            if (!ota_hdl && exec_wr_req->flags == BT_ATT_FLAG_EXEC) {
                config_update(config_get_src());
            }
            bt_att_cmd_exec_wr_rsp(device->acl_handle);
            break;
        }
        default:
            printf("# Unsupported OPCODE: 0x%02X\n", bt_hci_acl_pkt->att_hdr.code);
    }
}
