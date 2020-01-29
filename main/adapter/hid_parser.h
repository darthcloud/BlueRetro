#ifndef _HID_PARSER_H_
#define _HID_PARSER_H_

void hid_parser(uint8_t *data, uint32_t len);
int32_t hid_fingerprint(void);
int32_t hid_device_fingerprint(struct hid_report *report);

#endif /* _HID_PARSER_H_ */
