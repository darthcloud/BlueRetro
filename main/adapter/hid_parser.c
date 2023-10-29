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

#define HID_STACK_MAX 4

struct hid_stack_element {
    uint32_t report_size;
    uint32_t report_cnt;
    int32_t logical_min;
    int32_t logical_max;
    uint8_t usage_page;
};

static struct hid_report *reports[BT_MAX_DEV][HID_MAX_REPORT] = {0};

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

static void hid_process_report(struct bt_data *bt_data, struct hid_report *report) {
    report->type = hid_report_fingerprint(report);
#ifdef CONFIG_BLUERETRO_JSON_DBG
    printf("{\"log_type\": \"parsed_hid_report\", \"report_id\": %ld, \"report_tag\": %ld, \"usages\": [", report->id, report->tag);
    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        if (i) {
            printf(", ");
        }
        printf("{\"usage_page\": %ld, \"usage\": %ld, \"bit_offset\": %lu, \"bit_size\": %lu}",
            report->usages[i].usage_page, report->usages[i].usage, report->usages[i].bit_offset,
            report->usages[i].bit_size);
    }
    printf("]");
#else
    printf("# %ld %c ", report->id, (report->tag) ? 'O' : 'I');
    for (uint32_t i = 0; i < report->usage_cnt; i++) {
        printf("%02lX%02lX %lu %lu ", report->usages[i].usage_page, report->usages[i].usage,
            report->usages[i].bit_offset, report->usages[i].bit_size);
    }
#endif
    if (report->type != REPORT_NONE) {
#ifdef CONFIG_BLUERETRO_JSON_DBG
        printf(", \"report_type\": %ld", report->type);
#else
        printf("rtype: %ld", report->type);
#endif
        /* For output report we got to make a choice. */
        /* So we use the first one we find. */
        if (report->tag == HID_OUT && bt_data->reports[report->type].id == 0) {
            memcpy(&bt_data->reports[report->type], report, sizeof(bt_data->reports[0]));
        }
    }
#ifdef CONFIG_BLUERETRO_JSON_DBG
    printf("}\n");
#else
    printf("\n");
#endif
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

    wip_report[0] = heap_caps_malloc(sizeof(struct hid_report), MALLOC_CAP_32BIT);
    wip_report[1] = heap_caps_malloc(sizeof(struct hid_report), MALLOC_CAP_32BIT);

    memset(wip_report[0], 0, sizeof(struct hid_report));
    memset(wip_report[1], 0, sizeof(struct hid_report));

    for (uint32_t i = 0; i < HID_MAX_REPORT; i++) {
        if (reports[bt_data->base.pids->id][i]) {
            free(reports[bt_data->base.pids->id][i]);
            reports[bt_data->base.pids->id][i] = NULL;
        }
    }

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
            case HID_MI_OUTPUT: /* 0x91 */
                tag_idx = 1;
                /* Fallthrough */
            case HID_MI_INPUT: /* 0x81 */
                if (!(*desc & 0x01) && hid_stack[hid_stack_idx].usage_page != 0xFF && usage_list[0] != 0xFF && report_usage_idx[tag_idx] < REPORT_MAX_USAGE) {
                    if (hid_stack[hid_stack_idx].report_size == 1) {
                        if (hid_stack[hid_stack_idx].report_cnt > 32) {
                            uint32_t bit_cnt = hid_stack[hid_stack_idx].report_cnt;
                            uint32_t div = 32;
                            uint32_t usage = usage_list[0];

                            while (bit_cnt) {
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].usage_page = hid_stack[hid_stack_idx].usage_page;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].usage = usage;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].flags = *desc;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].bit_offset = report_bit_offset[tag_idx];
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].bit_size = div;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].logical_min = hid_stack[hid_stack_idx].logical_min;
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].logical_max = hid_stack[hid_stack_idx].logical_max;
                                report_bit_offset[tag_idx] += div;
                                usage += div;

                                bit_cnt -= div;
                                while (bit_cnt < div) {
                                    div /= 2;
                                }
                                ++report_usage_idx[tag_idx];
                            }
                        }
                        else {
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].usage_page = hid_stack[hid_stack_idx].usage_page;
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].usage = usage_list[0];
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].flags = *desc;
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].bit_offset = report_bit_offset[tag_idx];
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].bit_size = hid_stack[hid_stack_idx].report_cnt * hid_stack[hid_stack_idx].report_size;
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].logical_min = hid_stack[hid_stack_idx].logical_min;
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].logical_max = hid_stack[hid_stack_idx].logical_max;
                            report_bit_offset[tag_idx] += hid_stack[hid_stack_idx].report_cnt * hid_stack[hid_stack_idx].report_size;
                            ++report_usage_idx[tag_idx];
                        }
                    }
                    else {
                        uint32_t idx_end = report_usage_idx[tag_idx] + hid_stack[hid_stack_idx].report_cnt;
                        if (idx_end > REPORT_MAX_USAGE) {
                            idx_end = REPORT_MAX_USAGE;
                        }
                        for (uint32_t i = 0; report_usage_idx[tag_idx] < idx_end; ++i, ++report_usage_idx[tag_idx]) {
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].usage_page = hid_stack[hid_stack_idx].usage_page;
                            if (usage_idx < 2) {
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].usage = usage_list[0];
                            }
                            else {
                                wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].usage = usage_list[i];
                            }
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].flags = *desc;
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].bit_offset = report_bit_offset[tag_idx];
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].bit_size = hid_stack[hid_stack_idx].report_size;
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].logical_min = hid_stack[hid_stack_idx].logical_min;
                            wip_report[tag_idx]->usages[report_usage_idx[tag_idx]].logical_max = hid_stack[hid_stack_idx].logical_max;
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
                            hid_process_report(bt_data, wip_report[i]);
                            reports[bt_data->base.pids->id][report_idx++] = wip_report[i];
                            wip_report[i] = heap_caps_malloc(sizeof(struct hid_report), MALLOC_CAP_32BIT);
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
                hid_process_report(bt_data, wip_report[i]);
                reports[bt_data->base.pids->id][report_idx++] = wip_report[i];
            }
        }
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
