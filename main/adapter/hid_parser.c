/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <esp_heap_caps.h>
#include "adapter.h"
#include "hid_parser.h"
#include "zephyr/usb_hid.h"
#include "tools/util.h"
#include "bluetooth/mon.h"
#include "tests/cmds.h"

#define HID_STACK_MAX 4

struct hid_stack_element {
    uint32_t report_size;
    uint32_t report_cnt;
    int32_t logical_min;
    int32_t logical_max;
    uint32_t usage_page;
    uint32_t usage_max;
};

static struct hid_report *reports[BT_MAX_DEV][HID_MAX_REPORT] = {0};

/* List of usage we don't care about */
static uint32_t hid_usage_is_collection(uint32_t page, uint32_t usage) {
    switch (page) {
        case 0x01: /* Generic Desktop Ctrls */
            switch (usage) {
                case 0x01:
                case 0x02:
                case 0x04:
                case 0x05:
                case 0x06:
                case 0x07:
                case 0x08:
                case 0x09:
                case 0x3A:
                case 0x80:
                    return 1;
                default:
                    return 0;
            }
        case 0x02: /* Sim Ctrls */
            switch (usage) {
                case 0x01:
                case 0x02:
                case 0x03:
                case 0x04:
                case 0x05:
                case 0x06:
                case 0x07:
                case 0x08:
                case 0x09:
                case 0x0A:
                case 0x0B:
                case 0x0C:
                case 0x20:
                case 0x21:
                case 0x22:
                case 0x23:
                case 0x24:
                case 0x25:
                    return 1;
                default:
                    return 0;
            }
        case 0x03: /* VR Ctrls */
            switch (usage) {
                case 0x01:
                case 0x02:
                case 0x03:
                case 0x04:
                case 0x05:
                case 0x06:
                case 0x07:
                case 0x08:
                case 0x09:
                case 0x0A:
                    return 1;
                default:
                    return 0;
            }
        case 0x04: /* Sport Ctrls */
            switch (usage) {
                case 0x01:
                case 0x02:
                case 0x03:
                case 0x04:
                case 0x38:
                    return 1;
                default:
                    return 0;
            }
        case 0x05: /* Game Ctrls */
            switch (usage) {
                case 0x01:
                case 0x02:
                case 0x03:
                case 0x20:
                case 0x32:
                case 0x37:
                case 0x39:
                    return 1;
                default:
                    return 0;
            }
        case 0x0C: /* Consumer */
            switch (usage) {
                case 0x01:
                case 0x02:
                case 0x03:
                case 0x04:
                case 0x05:
                case 0x06:
                case 0x36:
                case 0x80:
                case 0x87:
                case 0xBA:
                case 0xF1:
                    return 1;
                default:
                    return 0;
            }
        case 0x0F: /* PID */
            switch (usage) {
                case 0x01:
                case 0x21:
                case 0x25:
                case 0x57:
                case 0x58:
                case 0x59:
                case 0x5A:
                case 0x5F:
                case 0x66:
                case 0x68:
                case 0x6B:
                case 0x6E:
                case 0x73:
                case 0x74:
                case 0x77:
                case 0x78:
                case 0x7D:
                case 0x7F:
                case 0x85:
                case 0x89:
                case 0x8B:
                case 0x90:
                case 0x91:
                case 0x92:
                case 0x95:
                case 0x96:
                case 0xA8:
                case 0xAB:
                    return 1;
                default:
                    return 0;
            }
        default:
            return 0;
    }
}

/* List of usage we care about */
static uint32_t hid_usage_is_used(uint32_t page, uint32_t usage) {
    switch (page) {
        case 0x01: /* Generic Desktop Ctrls */
            switch (usage) {
                case 0x30: /* X */
                case 0x31: /* Y */
                case 0x32: /* Z */
                case 0x33: /* RX */
                case 0x34: /* RY */
                case 0x35: /* RZ */
                case 0x36: /* Slider */
                case 0x37: /* Dial */
                case 0x38: /* Wheel */
                case 0x39: /* Hat */
                case 0x85: /* Sys Main Menu */
                    return 1;
                default:
                    return 0;
            }
        case 0x02: /* Sim Ctrls */
            switch (usage) {
                case 0xC4: /* Accel */
                case 0xC5: /* Brake */
                    return 1;
                default:
                    return 0;
            }
        case 0x07: /* Keyboard */
            return 1;
        case 0x09: /* Button */
            if (usage <= 20) {
                return 1;
            }
            return 0;
        case 0x0C: /* Consumer */
            switch (usage) {
                case 0x40 /* Menu */:
                case 0xB2 /* Record */:
                case 0x223 /* AC Home */:
                case 0x224 /* AC Back */:
                    return 1;
                default:
                    return 0;
            }
        case 0x0F: /* PID */
            switch (usage) {
                case 0x50: /* Duration */
                case 0x97: /* Enable Actuators */
                case 0x70: /* Magnitude */
                case 0x7C: /* Loop Count */
                case 0xA7: /* Start Delay */
                    return 1;
                default:
                    return 0;
            }
        default:
            return 0;
    }
}

static int32_t hid_report_fingerprint(struct hid_report *report) {
    int32_t type = REPORT_NONE;
    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        if (report->usages[i].usage_page) {
            switch (report->usages[i].usage_page) {
                case USAGE_GEN_DESKTOP:
                    switch (report->usages[i].usage) {
                        case USAGE_GEN_DESKTOP_X:
                        case USAGE_GEN_DESKTOP_Y:
                            if (report->usages[i].flags & 0x04) {
                                /* relative */
                                return MOUSE;
                            }
                            else {
                                return PAD;
                            }
                        case 0x39: /* HAT_SWITCH */
                            return PAD;
                        case 0x85: /* Sys Main Menu */
                            return MOUSE; /* Hack for xinput Xbox btn */
                    }
                    break;
                case USAGE_GEN_KEYBOARD:
                    type = KB;
                    break;
                case USAGE_GEN_BUTTON:
                    type = PAD;
                    break;
                case USAGE_GEN_PHYS_INPUT:
                    if ((report->usages[i].usage >= 0x95 && report->usages[i].usage <= 0x9C) ||
                         report->usages[i].usage == 0x50 || report->usages[i].usage == 0x70 ||
                         report->usages[i].usage == 0x7C || report->usages[i].usage == 0xA7) {
                        /*
                            0x50: Duration
                            0x70: Magnitude
                            0x7C: Loop count
                            0xA7: Start Delay
                            0x95: PID Device Control Report
                            0x96: PID Device Control
                            0x97: Enable Actuators
                            0x98: Disable Actuators
                            0x99: Stop all effects
                            0x9A: Device Reset
                            0x9B: Device Pause
                            0x9C: Device Continue
                            More info: https://www.usb.org/sites/default/files/hut1_4.pdf#page=213
                        */
                        return RUMBLE;
                    }
                    break;
            }
        }
        else {
            break;
        }
    }
    return type;
}

static void hid_patch_report(struct bt_data *bt_data, struct hid_report *report) {
    switch (bt_data->base.vid) {
        case 0x3250: /* Atari VCS */
        {
            uint32_t usage_cnt = 8;
            switch (bt_data->base.pid) {
                case 0x1001: /* Classic Controller */
                    usage_cnt = 4;
                    /* Fallthrough */
                case 0x1002: /* Modern Controller */
                    /* Rumble report */
                    if (report->id == 1 && report->tag == 1) {
                        uint8_t usages[] = {
                            0x70, 0x50, 0xA7, 0x7C, /* LF (left) */
                            0x70, 0x50, 0xA7, 0x7C, /* HF (right) */
                        };
                        report->usage_cnt = usage_cnt;
                        for (uint32_t i = 0; i < usage_cnt; i++) {
                            memset(&report->usages[i], 0, sizeof(report->usages[0]));
                            report->usages[i].usage_page = USAGE_GEN_PHYS_INPUT;
                            report->usages[i].usage = usages[i];
                            report->usages[i].logical_min = 0;
                            report->usages[i].logical_max = 0xFF;
                            report->usages[i].bit_size = 8;
                            report->usages[i].bit_offset = i * 8;
                        }
                    }
                    break;
            }
            break;
        }
    }
}

static void hid_process_report(struct bt_data *bt_data, struct hid_report *report) {
    hid_patch_report(bt_data, report);
    report->type = hid_report_fingerprint(report);
    TESTS_CMDS_LOG("{\"report_id\": %ld, \"report_tag\": %ld, \"usages\": [", report->id, report->tag);
    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        if (i) {
            TESTS_CMDS_LOG(", ");
        }
        TESTS_CMDS_LOG("{\"usage_page\": %ld, \"usage\": %ld, \"bit_offset\": %lu, \"bit_size\": %lu}",
            report->usages[i].usage_page, report->usages[i].usage, report->usages[i].bit_offset,
            report->usages[i].bit_size);
    }
    TESTS_CMDS_LOG("]");
    printf("%ld %c ", report->id, (report->tag) ? 'O' : 'I');
    bt_mon_log(false, "%ld %c ", report->id, (report->tag) ? 'O' : 'I');
    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        printf("%02lX%02lX %lu %lu ", report->usages[i].usage_page, report->usages[i].usage,
            report->usages[i].bit_offset, report->usages[i].bit_size);
        bt_mon_log(false, "%02lX%02lX %lu %lu ", report->usages[i].usage_page, report->usages[i].usage,
            report->usages[i].bit_offset, report->usages[i].bit_size);
    }
    if (report->type != REPORT_NONE) {
        TESTS_CMDS_LOG(", \"report_type\": %ld", report->type);
        printf("rtype: %ld", report->type);
        bt_mon_log(false, "rtype: %ld", report->type);
        /* For output report we got to make a choice. */
        /* So we use the first one we find. */
        if (report->tag == HID_OUT && bt_data->reports[report->type].id == 0) {
            memcpy(&bt_data->reports[report->type], report, sizeof(bt_data->reports[0]));
        }
    }
    TESTS_CMDS_LOG("}");
    printf("\n");
    bt_mon_log(true, "");
}

void hid_parser(struct bt_data *bt_data, uint8_t *data, uint32_t len) {
    struct hid_stack_element hid_stack[HID_STACK_MAX] = {0};
    uint8_t hid_stack_idx = 0;
    uint8_t usage_idx = 0;
    uint16_t usage_list[REPORT_MAX_USAGE] = {0};
    uint8_t *end = data + len;
    uint8_t *desc = data;
    uint8_t report_id = 0;
    uint8_t tag_idx = 0;
    uint32_t report_bit_offset[HID_TAG_CNT] = {0};
    uint32_t report_usage_idx[HID_TAG_CNT] = {0};
    uint8_t report_idx = 0;
    struct hid_report *wip_report[2] = {0};

    /* Free pre-existing reports for this device id */
    for (uint32_t i = 0; i < HID_MAX_REPORT; i++) {
        if (reports[bt_data->base.pids->id][i]) {
            free(reports[bt_data->base.pids->id][i]);
            reports[bt_data->base.pids->id][i] = NULL;
        }
    }

    wip_report[0] = heap_caps_aligned_alloc(32, sizeof(struct hid_report), MALLOC_CAP_32BIT);
    if (wip_report[0] == NULL) {
        printf("# %s: failed to alloc wip_report[0]\n", __FUNCTION__);
        return;
    }
    wip_report[1] = heap_caps_aligned_alloc(32, sizeof(struct hid_report), MALLOC_CAP_32BIT);
    if (wip_report[1] == NULL) {
        printf("# %s: failed to alloc wip_report[1]\n", __FUNCTION__);
        free(wip_report[0]);
        return;
    }

    memset(wip_report[0], 0, sizeof(struct hid_report));
    memset(wip_report[1], 0, sizeof(struct hid_report));


#ifdef CONFIG_BLUERETRO_DUMP_HID_DESC
    data_dump(data, len);
#endif
    TESTS_CMDS_LOG("\"hid_reports\": [");

    while (desc < end) {
        switch (*desc++) {
            case 0x00:
                break;
            case 0x01:
                desc++;
                break;
            case 0x03:
                desc += 4;
                break;
            case HID_GI_USAGE_PAGE: /* 0x05 */
                hid_stack[hid_stack_idx].usage_page = *desc++;
                break;
            case 0x06: /* USAGE_PAGE16 */
                hid_stack[hid_stack_idx].usage_page = *(uint16_t *)desc;
                desc += 2;
                break;
            case 0x07: /* USAGE_PAGE32 */
                hid_stack[hid_stack_idx].usage_page = *(uint32_t *)desc;
                desc += 4;
                break;
            case HID_LI_USAGE: /* 0x09 */
            case HID_LI_USAGE_MIN(1): /* 0x19 */
                if (!hid_usage_is_collection(hid_stack[hid_stack_idx].usage_page, *desc)) {
                    if (usage_idx < REPORT_MAX_USAGE) {
                        usage_list[usage_idx++] = *desc;
                    }
                }
                desc++;
                break;
            case 0x0A: /* USAGE16 */
            case HID_LI_USAGE_MIN(2): /* 0x1A */
                if (usage_idx < REPORT_MAX_USAGE) {
                    usage_list[usage_idx++] = *(uint16_t *)desc;
                }
                desc += 2;
                break;
            case 0x0D:
                desc++;
                break;
            case HID_GI_LOGICAL_MIN(1): /* 0x15 */
                hid_stack[hid_stack_idx].logical_min = *(int8_t *)desc++;
                break;
            case HID_GI_LOGICAL_MIN(2): /* 0x16 */
                hid_stack[hid_stack_idx].logical_min = *(int16_t *)desc;
                desc += 2;
                break;
            case HID_GI_LOGICAL_MIN(3): /* 0x17 */
                hid_stack[hid_stack_idx].logical_min = *(int32_t *)desc;
                desc += 4;
                break;
            case HID_GI_LOGICAL_MAX(1): /* 0x25 */
                hid_stack[hid_stack_idx].logical_max = *(int8_t *)desc++;
                break;
            case HID_GI_LOGICAL_MAX(2): /* 0x26 */
                hid_stack[hid_stack_idx].logical_max = *(int16_t *)desc;
                desc += 2;
                break;
            case HID_GI_LOGICAL_MAX(3): /* 0x27 */
                hid_stack[hid_stack_idx].logical_max = *(int32_t *)desc;
                desc += 4;
                break;
            case HID_LI_USAGE_MAX(1): /* 0x29 */
                hid_stack[hid_stack_idx].usage_max = *(int8_t *)desc++;
                break;
            case HID_LI_USAGE_MAX(2): /* 0x2A */
                hid_stack[hid_stack_idx].usage_max = *(int16_t *)desc;
                desc += 2;
                break;
            case HID_LI_USAGE_MAX(3): /* 0x2B */
                hid_stack[hid_stack_idx].usage_max = *(int32_t *)desc;
                desc += 4;
                break;
            case 0x35: /* PHYSICAL_MIN */
                desc++;
                break;
            case 0x45: /* PHYSICAL_MAX */
                desc++;
                break;
            case 0x46: /* PHYSICAL_MAX16 */
                desc += 2;
                break;
            case 0x55: /* UNIT_EXP */
                desc++;
                break;
            case 0x65: /* UNIT */
                desc++;
                break;
            case 0x66: /* UNIT16 */
                desc += 2;
                break;
            case HID_GI_REPORT_SIZE: /* 0x75 */
                hid_stack[hid_stack_idx].report_size = *desc++;
                break;
            case 0x7C:
                break;
            case HID_MI_OUTPUT: /* 0x91 */
                tag_idx = 1;
                /* Fallthrough */
            case HID_MI_INPUT: /* 0x81 */
                if (!(*desc & 0x01) && hid_stack[hid_stack_idx].usage_page != 0xFF && usage_list[0] != 0xFF && report_usage_idx[tag_idx] < REPORT_MAX_USAGE) {
                    bool bitfield_merge = (usage_idx == 1 && hid_stack[hid_stack_idx].report_size == 1 && hid_stack[hid_stack_idx].report_cnt > 1);
                    uint32_t usage_cnt = (bitfield_merge) ? 1 : hid_stack[hid_stack_idx].report_cnt;
                    for (uint32_t i = 0; i < usage_cnt; i++) {
                        if (hid_usage_is_used(hid_stack[hid_stack_idx].usage_page, usage_list[i])) {
                            uint32_t frag_cnt = 1;
                            if (bitfield_merge) {
                                frag_cnt = hid_stack[hid_stack_idx].report_cnt / 32;
                                if (hid_stack[hid_stack_idx].report_cnt % 32) {
                                    frag_cnt++;
                                }
                            }
                            for (uint32_t j = 0; j < frag_cnt; j++) {
                                uint32_t usage_offset = j * 32;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].usage_page = hid_stack[hid_stack_idx].usage_page;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].usage = (usage_idx == 1) ? usage_list[0] : usage_list[i];
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].usage += usage_offset;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].flags = *desc;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].bit_offset = report_bit_offset[tag_idx];
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].bit_offset += usage_offset;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].bit_size = hid_stack[hid_stack_idx].report_size;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].logical_min = hid_stack[hid_stack_idx].logical_min;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].logical_max = hid_stack[hid_stack_idx].logical_max;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].usage_max = hid_stack[hid_stack_idx].usage_max;
                                if (bitfield_merge) {
                                    wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].bit_size *= (frag_cnt > 1) ? 32 : hid_stack[hid_stack_idx].report_cnt;
                                }
                                if (frag_cnt > 1 && j == (frag_cnt - 1)) {
                                    wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].bit_size = hid_stack[hid_stack_idx].report_cnt % 32;
                                }
                                report_usage_idx[tag_idx]++;
                            }
                        }
                        if (bitfield_merge) {
                            report_bit_offset[tag_idx] += hid_stack[hid_stack_idx].report_cnt * hid_stack[hid_stack_idx].report_size;
                        }
                        else {
                            report_bit_offset[tag_idx] += hid_stack[hid_stack_idx].report_size;
                        }
                    }
                }
                else {
                    report_bit_offset[tag_idx] += hid_stack[hid_stack_idx].report_size * hid_stack[hid_stack_idx].report_cnt;
                }
                usage_idx = 0;
                tag_idx = 0;
                memset(usage_list, 0xFF, sizeof(usage_list));
                desc++;
                break;
            case HID_GI_REPORT_ID: /* 0x85 */
                /* process previous report fingerprint */
                if (report_id) {
                    for (uint32_t i = 0; i < HID_TAG_CNT; i++) {
                        wip_report[i]->tag = i;
                        wip_report[i]->usage_cnt = report_usage_idx[i];
                        wip_report[i]->len = report_bit_offset[i] / 8;
                        if (report_bit_offset[i] % 8) {
                            wip_report[i]->len++;
                        }
                        if (wip_report[i]->len) {
                            if (report_idx) {
                                TESTS_CMDS_LOG(",\n");
                            }
                            hid_process_report(bt_data, wip_report[i]);
                            reports[bt_data->base.pids->id][report_idx++] = wip_report[i];
                            wip_report[i] = heap_caps_aligned_alloc(32, sizeof(struct hid_report), MALLOC_CAP_32BIT);
                            if (wip_report[i] == NULL) {
                                return;
                            }
                            memset(wip_report[i], 0, sizeof(struct hid_report));
                        }
                    }
                }
                report_id = *desc++;
                for (uint32_t i = 0; i < HID_TAG_CNT; i++) {
                    wip_report[i]->id = report_id;
                    report_usage_idx[i] = 0;
                    report_bit_offset[i] = 0;
                }
                break;
            case HID_GI_REPORT_COUNT: /* 0x95 */
                hid_stack[hid_stack_idx].report_cnt = *desc++;
                break;
            case 0x96: /* REPORT_COUNT16 */
                hid_stack[hid_stack_idx].report_cnt = *(uint16_t *)desc;
                desc += 2;
                break;
            case HID_MI_COLLECTION: /* 0xA1 */
                desc++;
                break;
            case 0xA4: /* PUSH */
                if (hid_stack_idx < (HID_STACK_MAX - 1)) {
                    memcpy(&hid_stack[hid_stack_idx + 1], &hid_stack[hid_stack_idx], sizeof(hid_stack[0]));
                    hid_stack_idx++;
                }
                else {
                    printf("# %s HID stack overflow\n", __FUNCTION__);
                }
                break;
            case 0xB1: /* FEATURE */
                usage_idx = 0;
                desc++;
                break;
            case 0xB2: /* FEATURE16 */
                usage_idx = 0;
                desc += 2;
                break;
            case 0xB4: /* POP */
                if (hid_stack_idx > 0) {
                    hid_stack_idx--;
                }
                else {
                    printf("# %s HID stack underrun\n", __FUNCTION__);
                }
                break;
            case HID_MI_COLLECTION_END: /* 0xC0 */
                break;
            case 0xFF:
                desc += 4;
                break;
            default:
                printf("# Unknown HID marker: %02X\n", *--desc);
                return;
        }
    }
    if (report_idx == 0 && wip_report[HID_IN]->id == 0) {
        report_id = 1;
        wip_report[HID_IN]->id = 1;
        wip_report[HID_OUT]->id = 1;
    }
    if (report_id) {
        for (uint32_t i = 0; i < HID_TAG_CNT; i++) {
            wip_report[i]->tag = i;
            wip_report[i]->usage_cnt = report_usage_idx[i];
            wip_report[i]->len = report_bit_offset[i] / 8;
            if (report_bit_offset[i] % 8) {
                wip_report[i]->len++;
            }
            if (wip_report[i]->len) {
                if (report_idx) {
                    TESTS_CMDS_LOG(",\n");
                }
                hid_process_report(bt_data, wip_report[i]);
                reports[bt_data->base.pids->id][report_idx++] = wip_report[i];
            }
        }
    }
    TESTS_CMDS_LOG("],\n");

    /* Free reports for non-generic devices */
    if (bt_data->base.pids->type > BT_HID_GENERIC) {
        hid_parser_free_reports(bt_data->base.pids->id);
    }
}

struct hid_report *hid_parser_get_report(int32_t dev_id, uint8_t report_id) {
    struct hid_report **our_reports = reports[dev_id];

    for (uint32_t i = 0; i < HID_MAX_REPORT; i++) {
        struct hid_report *report = our_reports[i];
        if (report && report->len && report->id == report_id && report->tag == HID_IN) {
            return report;
        }
    }
    return NULL;
}

void hid_parser_load_report(struct bt_data *bt_data, uint8_t report_id) {
    struct hid_report **our_reports = reports[bt_data->base.pids->id];

    for (uint32_t i = 0; i < HID_MAX_REPORT; i++) {
        struct hid_report *report = our_reports[i];
        if (report && report->len && report->id == report_id && report->tag == HID_IN) {
            if (report->type != REPORT_NONE) {
                memcpy(&bt_data->reports[report->type], report, sizeof(bt_data->reports[0]));
            }
        }
    }
}

void hid_parser_free_reports(int32_t dev_id) {
    struct hid_report **our_reports = reports[dev_id];

    for (uint32_t i = 0; i < HID_MAX_REPORT; i++) {
        struct hid_report *report = our_reports[i];
        if (report) {
            free(report);
            our_reports[i] = NULL;
        }
    }
}
