/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "adapter.h"
#include "hid_parser.h"
#include "zephyr/usb_hid.h"
#include "tools/util.h"

#define HID_STACK_MAX 4

struct hid_stack_element {
    uint32_t report_size;
    uint32_t report_cnt;
    int32_t logical_min;
    int32_t logical_max;
    uint8_t usage_page;
};

struct hid_fingerprint {
    int32_t dev_type;
    uint32_t dev_subtype;
    uint32_t fp_len;
    uint8_t fp[REPORT_MAX_USAGE*2];
};

static const struct hid_fingerprint hid_fp[] = {
#ifndef CONFIG_BLUERETRO_GENERIC_HID_DEBUG
    {
        .dev_type = BT_PS3,
        .dev_subtype = BT_SUBTYPE_DEFAULT,
        .fp_len = 10,
        .fp = {0x09, 0x01, 0x01, 0x30, 0x01, 0x31, 0x01, 0x32, 0x01, 0x35},
    },
    {
        .dev_type = BT_PS,
        .dev_subtype = BT_SUBTYPE_DEFAULT,
        .fp_len = 16,
        .fp = {0x01, 0x30, 0x01, 0x31, 0x01, 0x32, 0x01, 0x35, 0x01, 0x39, 0x09, 0x01, 0x01, 0x33, 0x01, 0x34},
    },
    {
        .dev_type = BT_XBOX,
        .dev_subtype = BT_XBOX_XINPUT,
        .fp_len = 16,
        .fp = {0x01, 0x30, 0x01, 0x31, 0x01, 0x33, 0x01, 0x34, 0x01, 0x32, 0x01, 0x35, 0x01, 0x39, 0x09, 0x01},
    },
    {
        .dev_type = BT_XBOX,
        .dev_subtype = BT_XBOX_XS,
        .fp_len = 16,
        .fp = {0x01, 0x30, 0x01, 0x31, 0x01, 0x32, 0x01, 0x35, 0x02, 0xC5, 0x02, 0xC4, 0x01, 0x39, 0x09, 0x01, 0x0C, 0xB2},
    },
    {
        .dev_type = BT_SW,
        .dev_subtype = BT_SUBTYPE_DEFAULT,
        .fp_len = 12,
        .fp = {0x09, 0x01, 0x01, 0x39, 0x01, 0x30, 0x01, 0x31, 0x01, 0x33, 0x01, 0x34},
    },
#endif
};

/* List of usage we don't care about */
static uint32_t hid_usage_is_collection(uint8_t page, uint16_t usage) {
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
        default:
            return 0;
    }
}

static int32_t hid_report_fingerprint(struct hid_report *report) {
    int32_t type = REPORT_NONE;
    for (uint32_t i = 0; i < REPORT_MAX_USAGE; i++) {
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
                    }
                    break;
                case USAGE_GEN_KEYBOARD:
                    return KB;
                case USAGE_GEN_BUTTON:
                    type = PAD;
                    break;
                case 0x0C: /* CONSUMER */
                    if (type == REPORT_NONE) {
                        type = EXTRA;
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

static void hid_device_fingerprint(struct hid_report *report, int32_t *type, uint32_t *subtype) {
    uint8_t fp[REPORT_MAX_USAGE*2] = {0};
    uint32_t fp_len = 0;

    switch (hid_report_fingerprint(report)) {
        case KB:
        case MOUSE:
            *type = BT_HID_GENERIC;
            *subtype = BT_SUBTYPE_DEFAULT;
            return;
        case PAD:
            for (uint32_t i = 0; i < REPORT_MAX_USAGE; i++) {
                if (report->usages[i].usage_page) {
                    fp[fp_len++] = report->usages[i].usage_page;
                    fp[fp_len++] = report->usages[i].usage;
                }
                else {
                    break;
                }
            }
            for (uint32_t i = 0; i < sizeof(hid_fp)/sizeof(hid_fp[0]); i++) {
                if (memcmp(hid_fp[i].fp, fp, fp_len) == 0) {
                    *type = hid_fp[i].dev_type;
                    *subtype = hid_fp[i].dev_subtype;
                    return;
                }
            }
            *type = BT_HID_GENERIC;
            *subtype = BT_SUBTYPE_DEFAULT;
            return;
    }
    *type = BT_NONE;
    *subtype = BT_SUBTYPE_DEFAULT;
    return;
}

static void hid_process_report(struct bt_data *bt_data, struct hid_report *wip_report, uint32_t report_bit_offset, uint32_t report_usage_idx) {
    int32_t report_type = REPORT_NONE;
    int32_t dev_type = BT_NONE;
    uint32_t dev_subtype = BT_SUBTYPE_DEFAULT;

    wip_report->len = report_bit_offset / 8;
    wip_report->usage_cnt = report_usage_idx;
    if (report_bit_offset % 8) {
        wip_report->len++;
    }
    report_type = hid_report_fingerprint(wip_report);
    if (report_type != REPORT_NONE && bt_data->reports[report_type].id == 0) {
        hid_device_fingerprint(wip_report, &dev_type, &dev_subtype);
        memcpy(&bt_data->reports[report_type], wip_report, sizeof(bt_data->reports[0]));
        if (bt_data->base.pids->type <= BT_HID_GENERIC && dev_type > BT_HID_GENERIC) {
            bt_type_update(bt_data->base.pids->id, dev_type, dev_subtype);
        }
        printf("rtype: %ld dtype: %ld sub: %ld", report_type, dev_type, dev_subtype);
    }
    printf("\n");
}

void hid_parser(struct bt_data *bt_data, uint8_t *data, uint32_t len) {
    struct hid_stack_element hid_stack[HID_STACK_MAX] = {0};
    uint8_t hid_stack_idx = 0;
    uint8_t usage_idx = 0;
    struct hid_report wip_report;
    uint16_t usage_list[REPORT_MAX_USAGE] = {0};
    uint8_t *end = data + len;
    uint8_t *desc = data;
    uint8_t report_id = 0;
    uint8_t report_cnt = 0;
    uint32_t report_bit_offset = 0;
    uint32_t report_usage_idx = 0;

#ifdef CONFIG_BLUERETRO_DUMP_HID_DESC
    data_dump(data, len);
#endif

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
                hid_stack[hid_stack_idx].usage_page = 0xFF;
                desc += 2;
                break;
            case 0x07: /* USAGE_PAGE32 */
                hid_stack[hid_stack_idx].usage_page = 0xFF;
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
                if (usage_idx < REPORT_MAX_USAGE) {
                    usage_list[usage_idx++] = *(uint16_t *)desc;
                }
                desc += 2;
                break;
            case 0x0D:
                desc++;
                break;
            case HID_GI_LOGICAL_MIN(1): /* 0x15 */
                hid_stack[hid_stack_idx].logical_min = *desc++;
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
                hid_stack[hid_stack_idx].logical_max = *desc++;
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
                desc++;
                break;
            case HID_LI_USAGE_MAX(2): /* 0x2A */
                desc += 2;
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
            case HID_MI_INPUT: /* 0x81 */
                if (!(*desc & 0x01) && hid_stack[hid_stack_idx].usage_page != 0xFF && usage_list[0] != 0xFF && report_usage_idx < REPORT_MAX_USAGE) {
                    if (hid_stack[hid_stack_idx].report_size == 1) {
                        wip_report.usages[report_usage_idx].usage_page = hid_stack[hid_stack_idx].usage_page;
                        wip_report.usages[report_usage_idx].usage = usage_list[0];
                        wip_report.usages[report_usage_idx].flags = *desc;
                        wip_report.usages[report_usage_idx].bit_offset = report_bit_offset;
                        wip_report.usages[report_usage_idx].bit_size = hid_stack[hid_stack_idx].report_cnt * hid_stack[hid_stack_idx].report_size;
                        wip_report.usages[report_usage_idx].logical_min = hid_stack[hid_stack_idx].logical_min;
                        wip_report.usages[report_usage_idx].logical_max = hid_stack[hid_stack_idx].logical_max;
                        printf("%02X%02X %lu %lu ", hid_stack[hid_stack_idx].usage_page, usage_list[0], report_bit_offset, hid_stack[hid_stack_idx].report_cnt * hid_stack[hid_stack_idx].report_size);
                        report_bit_offset += hid_stack[hid_stack_idx].report_cnt * hid_stack[hid_stack_idx].report_size;
                        ++report_usage_idx;
                    }
                    else {
                        uint32_t idx_end = report_usage_idx + hid_stack[hid_stack_idx].report_cnt;
                        if (idx_end > REPORT_MAX_USAGE) {
                            idx_end = REPORT_MAX_USAGE;
                        }
                        for (uint32_t i = 0; report_usage_idx < idx_end; ++i, ++report_usage_idx) {
                            wip_report.usages[report_usage_idx].usage_page = hid_stack[hid_stack_idx].usage_page;
                            printf("%02X", hid_stack[hid_stack_idx].usage_page);
                            if (usage_idx < 2) {
                                wip_report.usages[report_usage_idx].usage = usage_list[0];
                                printf("%02X ", usage_list[0]);
                            }
                            else {
                                wip_report.usages[report_usage_idx].usage = usage_list[i];
                                printf("%02X ", usage_list[i]);
                            }
                            wip_report.usages[report_usage_idx].flags = *desc;
                            wip_report.usages[report_usage_idx].bit_offset = report_bit_offset;
                            wip_report.usages[report_usage_idx].bit_size = hid_stack[hid_stack_idx].report_size;
                            wip_report.usages[report_usage_idx].logical_min = hid_stack[hid_stack_idx].logical_min;
                            wip_report.usages[report_usage_idx].logical_max = hid_stack[hid_stack_idx].logical_max;
                            printf("%lu %lu, ", report_bit_offset, hid_stack[hid_stack_idx].report_size);
                            report_bit_offset += hid_stack[hid_stack_idx].report_size;
                        }
                    }
                }
                else {
                    report_bit_offset += hid_stack[hid_stack_idx].report_size * hid_stack[hid_stack_idx].report_cnt;
                }
                usage_idx = 0;
                memset(usage_list, 0xFF, sizeof(usage_list));
                desc++;
                break;
            case HID_GI_REPORT_ID: /* 0x85 */
                /* process previous report fingerprint */
                if (report_id) {
                    hid_process_report(bt_data, &wip_report, report_bit_offset, report_usage_idx);
                    report_cnt++;
                }
                memset((void *)&wip_report, 0, sizeof(wip_report));
                report_id = *desc++;
                wip_report.id = report_id;
                report_usage_idx = 0;
                report_bit_offset = 0;
                printf("# %d ", report_id);
                break;
            case HID_MI_OUTPUT: /* 0x91 */
                usage_idx = 0;
                desc++;
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
                    printf("%s HID stack overflow\n", __FUNCTION__);
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
                    printf("%s HID stack underrun\n", __FUNCTION__);
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
    if (report_cnt == 0) {
        report_id = 1;
    }
    if (report_id) {
        hid_process_report(bt_data, &wip_report, report_bit_offset, report_usage_idx);
    }
}
