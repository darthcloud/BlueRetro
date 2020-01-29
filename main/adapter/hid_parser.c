#include <stdio.h>
#include <string.h>
#include "adapter.h"
#include "hid_parser.h"
#include "../zephyr/usb_hid.h"

struct hid_fingerprint {
    int32_t dev_type;
    uint32_t fp_len;
    uint8_t fp[REPORT_MAX_USAGE*2];
};

static const struct hid_fingerprint hid_fp[] = {
    {
        .dev_type = PS3_DS3,
        .fp_len = 10,
        .fp = {0x09, 0x01, 0x01, 0x30, 0x01, 0x31, 0x01, 0x32, 0x01, 0x35},
    },
    {
        .dev_type = PS4_DS4,
        .fp_len = 16,
        .fp = {0x01, 0x30, 0x01, 0x31, 0x01, 0x32, 0x01, 0x35, 0x01, 0x39, 0x09, 0x01, 0x01, 0x33, 0x01, 0x34},
    },
    {
        .dev_type = XB1_S,
        .fp_len = 16,
        .fp = {0x01, 0x30, 0x01, 0x31, 0x01, 0x33, 0x01, 0x34, 0x01, 0x32, 0x01, 0x35, 0x01, 0x39, 0x09, 0x01},
    },
    {
        .dev_type = XB1_S,
        .fp_len = 16,
        .fp = {0x01, 0x30, 0x01, 0x31, 0x01, 0x32, 0x01, 0x35, 0x02, 0xC5, 0x02, 0xC4, 0x01, 0x39, 0x09, 0x01},
    },
    {
        .dev_type = SWITCH_PRO,
        .fp_len = 12,
        .fp = {0x09, 0x01, 0x01, 0x39, 0x01, 0x30, 0x01, 0x31, 0x01, 0x33, 0x01, 0x34},
    },
};

/* List of usage we don't care about */
static uint32_t hid_usage_is_collection(uint8_t page, uint8_t usage) {
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
                        case 0x85: /* Menu */
                            return EXTRA;
                    }
                    break;
                case USAGE_GEN_KEYBOARD:
                    return KB;
                case USAGE_GEN_BUTTON:
                    type = PAD;
                    break;
            }
        }
        else {
            break;
        }
    }
    return type;
}

static int32_t hid_device_fingerprint(struct hid_report *report) {
    uint8_t fp[REPORT_MAX_USAGE*2] = {0};
    int32_t type = BT_NONE;
    uint32_t fp_len = 0;

    switch (hid_report_fingerprint(report)) {
        case KB:
            return HID_KB;
        case MOUSE:
            return HID_MOUSE;
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
                    return hid_fp[i].dev_type;
                }
            }
            return HID_PAD;
    }
    return type;
}

void hid_parser(struct bt_data *bt_data, uint8_t *data, uint32_t len) {
    struct hid_report wip_report;
    uint8_t usage_list[REPORT_MAX_USAGE] = {0};
    uint8_t *end = data + len;
    uint8_t *desc = data;
    uint8_t usage_page = 0;
    uint8_t *usage = usage_list;
    int32_t report_type = REPORT_NONE;
    int32_t dev_type = BT_NONE;
    uint8_t report_id = 0;
    uint32_t report_size = 0;
    uint32_t report_cnt = 0;
    int32_t logical_min = 0;
    int32_t logical_max = 0;
    uint32_t report_bit_offset = 0;
    uint32_t report_usage_idx = 0;

    while (desc < end) {
        switch (*desc++) {
            case HID_GI_USAGE_PAGE: /* 0x05 */
                usage_page = *desc++;
                break;
            case 0x06: /* USAGE_PAGE16 */
                usage_page = 0xFF;
                desc += 2;
                break;
            case HID_LI_USAGE: /* 0x09 */
            case HID_LI_USAGE_MIN(1): /* 0x19 */
                if (!hid_usage_is_collection(usage_page, *desc)) {
                    *usage++ = *desc;
                }
                desc++;
                break;
            case 0x0A: /* USAGE16 */
                *usage++ = 0xFF;
                desc += 2;
                break;
            case HID_GI_LOGICAL_MIN(1): /* 0x15 */
                logical_min = *desc++;
                break;
            case HID_GI_LOGICAL_MIN(2): /* 0x16 */
                logical_min = *(int16_t *)desc;
                desc += 2;
                break;
            case HID_GI_LOGICAL_MIN(3): /* 0x17 */
                logical_min = *(int32_t *)desc;
                desc += 4;
                break;
            case HID_GI_LOGICAL_MAX(1): /* 0x25 */
                logical_max = *desc++;
                break;
            case HID_GI_LOGICAL_MAX(2): /* 0x26 */
                logical_max = *(int16_t *)desc;
                desc += 2;
                break;
            case HID_GI_LOGICAL_MAX(3): /* 0x27 */
                logical_max = *(int32_t *)desc;
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
                report_size = *desc++;
                break;
            case HID_MI_INPUT: /* 0x81 */
                if (!(*desc & 0x01) && usage_page != 0xFF && usage_list[0] != 0xFF && report_usage_idx < REPORT_MAX_USAGE) {
                    if (report_size == 1) {
                        wip_report.usages[report_usage_idx].usage_page = usage_page;
                        wip_report.usages[report_usage_idx].usage = usage_list[0];
                        wip_report.usages[report_usage_idx].flags = *desc;
                        wip_report.usages[report_usage_idx].bit_offset = report_bit_offset;
                        wip_report.usages[report_usage_idx].bit_size = report_cnt * report_size;
                        wip_report.usages[report_usage_idx].logical_min = logical_min;
                        wip_report.usages[report_usage_idx].logical_max = logical_max;
                        printf("%02X%02X %u %u ", usage_page, usage_list[0], report_bit_offset, report_cnt * report_size);
                        report_bit_offset += report_cnt * report_size;
                        ++report_usage_idx;
                    }
                    else {
                        uint32_t idx_end = report_usage_idx + report_cnt;
                        if (idx_end > REPORT_MAX_USAGE) {
                            idx_end = REPORT_MAX_USAGE;
                        }
                        for (uint32_t i = 0; report_usage_idx < idx_end; ++i, ++report_usage_idx) {
                            wip_report.usages[report_usage_idx].usage_page = usage_page;
                            printf("%02X", usage_page);
                            if (usage == usage_list || usage == usage_list+1) {
                                wip_report.usages[report_usage_idx].usage = usage_list[0];
                                printf("%02X ", usage_list[0]);
                            }
                            else {
                                wip_report.usages[report_usage_idx].usage = usage_list[i];
                                printf("%02X ", usage_list[i]);
                            }
                            wip_report.usages[report_usage_idx].flags = *desc;
                            wip_report.usages[report_usage_idx].bit_offset = report_bit_offset;
                            wip_report.usages[report_usage_idx].bit_size = report_size;
                            wip_report.usages[report_usage_idx].logical_min = logical_min;
                            wip_report.usages[report_usage_idx].logical_max = logical_max;
                            printf("%u %u, ", report_bit_offset, report_size);
                            report_bit_offset += report_size;
                        }
                    }
                }
                else {
                    report_bit_offset += report_size * report_cnt;
                }
                usage = usage_list;
                memset(usage_list, 0xFF, sizeof(usage_list));
                desc++;
                break;
            case HID_GI_REPORT_ID: /* 0x85 */
                /* process previous report fingerprint */
                if (report_id) {
                    report_type = hid_report_fingerprint(&wip_report);
                    if (report_type != REPORT_NONE) {
                        dev_type = hid_device_fingerprint(&wip_report);
                        memcpy((void *)&bt_data->reports[report_type], (void *)&wip_report, sizeof(bt_data->reports[report_type]));
                        printf("rtype: %d dtype: %d", report_type, dev_type);
                    }
                    printf("\n");
                }
                memset((void *)&wip_report, 0, sizeof(wip_report));
                report_id = *desc++;
                wip_report.id = report_id;
                report_usage_idx = 0;
                report_bit_offset = 0;
                printf("# %d ", report_id);
                break;
            case HID_MI_OUTPUT: /* 0x91 */
                usage = usage_list;
                desc++;
                break;
            case HID_GI_REPORT_COUNT: /* 0x95 */
                report_cnt = *desc++;
                break;
            case 0x96: /* REPORT_COUNT16 */
                report_cnt = *(uint16_t *)desc;
                desc += 2;
                break;
            case HID_MI_COLLECTION: /* 0xA1 */
                desc++;
                break;
            case 0xB1: /* FEATURE */
                usage = usage_list;
                desc++;
                break;
            case HID_MI_COLLECTION_END: /* 0xC0 */
                break;
            default:
                printf("# Unknown HID marker: %02X\n", *--desc);
                return;
        }
    }
    if (report_id) {
        report_type = hid_report_fingerprint(&wip_report);
        if (report_type != REPORT_NONE) {
            dev_type = hid_device_fingerprint(&wip_report);
            memcpy((void *)&bt_data->reports[report_type], (void *)&wip_report, sizeof(bt_data->reports[report_type]));
            printf("rtype: %d dtype: %d", report_type, dev_type);
        }
        printf("\n");
    }
}
