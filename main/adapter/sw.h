#ifndef _SW_H_
#define _SW_H_
#include "adapter.h"

void sw_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data);
void sw_fb_from_generic(struct generic_fb *fb_data, struct bt_data *bt_data);

#endif /* _SW_H_ */
