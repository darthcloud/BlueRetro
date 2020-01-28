#include <stdio.h>
#include <string.h>
#include "adapter.h"
#include "hid_parser.h"
#include "../zephyr/usb_hid.h"

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

void hid_parser(uint8_t *data, uint32_t len) {
    uint8_t *end = data + len;
    uint8_t *ptr = data;
    uint8_t usage_page = 0;
    uint8_t usage = 0;
    uint8_t report_id = 0;
    uint8_t hid_fingerprint[64];
    uint8_t *ptr_fp = hid_fingerprint;

    while (ptr < end) {
        switch (*ptr++) {
            case HID_GI_USAGE_PAGE: /* 0x05 */
                usage_page = *ptr++;
                break;
            case 0x06: /* USAGE_PAGE16 */
                ptr += 2;
                break;
            case HID_LI_USAGE: /* 0x09 */
            case HID_LI_USAGE_MIN(1): /* 0x19 */
                usage = *ptr++;
                if (!hid_usage_is_collection(usage_page, usage)) {
                    *ptr_fp++ = usage_page;
                    *ptr_fp++ = usage;
                }
                break;
            case 0x0A: /* USAGE16 */
                ptr += 2;
                break;
            case HID_GI_LOGICAL_MIN(1): /* 0x15 */
                ptr++;
                break;
            case HID_GI_LOGICAL_MIN(2): /* 0x16 */
                ptr += 2;
                break;
            case HID_GI_LOGICAL_MIN(3): /* 0x17 */
                ptr += 4;
                break;
            case HID_GI_LOGICAL_MAX(1): /* 0x25 */
                ptr++;
                break;
            case HID_GI_LOGICAL_MAX(2): /* 0x26 */
                ptr += 2;
                break;
            case HID_GI_LOGICAL_MAX(3): /* 0x27 */
                ptr += 4;
                break;
            case HID_LI_USAGE_MAX(1): /* 0x29 */
                ptr++;
                break;
            case HID_LI_USAGE_MAX(2): /* 0x2A */
                ptr += 2;
                break;
            case 0x35: /* PHYSICAL_MIN */
                ptr++;
                break;
            case 0x45: /* PHYSICAL_MAX */
                ptr++;
                break;
            case 0x46: /* PHYSICAL_MAX16 */
                ptr += 2;
                break;
            case 0x55: /* UNIT_EXP */
                ptr++;
                break;
            case 0x65: /* UNIT */
                ptr++;
                break;
            case 0x66: /* UNIT16 */
                ptr += 2;
                break;
            case HID_GI_REPORT_SIZE: /* 0x75 */
                ptr++;
                break;
            case HID_MI_INPUT: /* 0x81 */
                ptr++;
                break;
            case HID_GI_REPORT_ID: /* 0x85 */
                /* process previous report fingerprint */
                if (report_id) {
                    printf("# %d ", report_id);
                    for (uint8_t *p = hid_fingerprint; *p != 0; p++) {
                        printf("%02X", *p);
                    }
                    printf("\n");
                }
                memset(hid_fingerprint, 0 , sizeof(hid_fingerprint));
                ptr_fp = hid_fingerprint;
                report_id = *ptr++;
                break;
            case HID_MI_OUTPUT: /* 0x91 */
                ptr++;
                break;
            case HID_GI_REPORT_COUNT: /* 0x95 */
                ptr++;
                break;
            case 0x96: /* REPORT_COUNT16 */
                ptr += 2;
                break;
            case HID_MI_COLLECTION: /* 0xA1 */
                ptr++;
                break;
            case 0xB1: /* FEATURE */
                ptr++;
                break;
            case HID_MI_COLLECTION_END: /* 0xC0 */
                break;
            default:
                printf("# Unknown HID marker: %02X\n", *--ptr);
                return;
        }
    }
    if (report_id) {
        printf("# %d ", report_id);
        for (uint8_t *p = hid_fingerprint; *p != 0; p++) {
            printf("%02X", *p);
        }
        printf("\n");
        report_id = 0;
    }
}
