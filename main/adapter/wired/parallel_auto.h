#ifndef _PARA_AUTO_H_
#define _PARA_AUTO_H_
#include "adapter/adapter.h"

void para_auto_meta_init(struct generic_ctrl *ctrl_data);
void para_auto_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void para_auto_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data);

#endif /* _PARA_AUTO_H_ */
