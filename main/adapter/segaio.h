#ifndef _SEGAIO_H_
#define _SEGAIO_H_
#include "adapter.h"

void segaio_meta_init(int32_t dev_mode, struct generic_ctrl *ctrl_data);
void segaio_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void segaio_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data);

#endif /* _SEGAIO_H_ */
