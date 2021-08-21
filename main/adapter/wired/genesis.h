#ifndef _GENESIS_H_
#define _GENESIS_H_
#include "adapter/adapter.h"

void genesis_meta_init(struct generic_ctrl *ctrl_data);
void genesis_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void genesis_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data);

#endif /* _GENESIS_H_ */
