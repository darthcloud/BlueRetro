#include <stdio.h>
#include <string.h>
#include "adapter.h"
#include "hid_parser.h"
#include "../zephyr/usb_hid.h"

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
                *ptr_fp++ = usage_page;
                *ptr_fp++ = usage;
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
            case HID_GI_LOGICAL_MAX(1): /* 0x25 */
                ptr++;
                break;
            case HID_GI_LOGICAL_MAX(2): /* 0x26 */
                ptr++;
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
            case HID_GI_REPORT_SIZE: /* 0x75 */
                ptr++;
                break;
            case HID_MI_INPUT: /* 0x81 */
                ptr++;
                break;
            case HID_GI_REPORT_ID: /* 0x85 */
                /* process previous report fingerprint */
                if (report_id) {
                    printf("# %d %s\n", report_id, hid_fingerprint);
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
}
