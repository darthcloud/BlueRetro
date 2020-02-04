#ifndef _GC_H_
#define _GC_H_
#include "adapter.h"

void gc_meta_init(int32_t dev_mode, struct generic_ctrl *ctrl_data);
void gc_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void gc_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data);
void gc_fb_to_generic(int32_t dev_mode, uint8_t *raw_fb_data, uint32_t raw_fb_len, struct generic_fb *fb_data);

#endif /* _GC_H_ */
