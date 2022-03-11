#ifndef _SEA_H_
#define _SEA_H_
#include "adapter/adapter.h"

void sea_meta_init(struct generic_ctrl *ctrl_data);
void sea_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void sea_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data);

#endif /* _SEA_H_ */
