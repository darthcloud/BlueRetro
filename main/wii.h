#ifndef _WII_H_
#define _WII_H_
#include "adapter.h"

extern const uint32_t wiiu_btns_mask[32];

void wiiu_init_desc(struct bt_data *bt_data);
void wiiu_to_generic(struct bt_data *bt_data, struct generic_ctrl *ctrl_data);
#endif /* _WII_H_ */
