#ifndef _PARA_1P_H_
#define _PARA_1P_H_
#include "adapter.h"

void para_1p_meta_init(int32_t dev_mode, struct generic_ctrl *ctrl_data);
void para_1p_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void para_1p_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data);

#endif /* _PARA_1P_H_ */
