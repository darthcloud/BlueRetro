#ifndef _INPUT_H_
#define _INPUT_H_

struct bt_hidp_wiiu_pro {
    uint16_t ls_x_axis;
    uint16_t rs_x_axis;
    uint16_t ls_y_axis;
    uint16_t rs_y_axis;
    uint8_t buttons[3];
} __packed;

#define INPUT_FORMAT_WIIU_PRO 0x00
struct input_data {
    uint8_t format;
    union {
        struct bt_hidp_wiiu_pro wiiu_pro;
    } data;
} __packed;

#endif /* _INPUT_H_ */

