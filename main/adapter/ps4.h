#ifndef _PS4_H_
#define _PS4_H_
#include "adapter.h"

void ps4_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data);
void ps4_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data);

#endif /* _PS4_H_ */
