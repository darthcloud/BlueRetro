#ifndef _N64_H_
#define _N64_H_
#include "adapter.h"

void n64_meta_init(struct generic_ctrl *ctrl_data);
void n64_init_buffer(struct wired_data *wired_data);
void n64_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data);
void n64_fb_to_generic(uint8_t *raw_fb_data, uint32_t raw_fb_len, struct generic_fb *fb_data);

#endif /* _N64_H_ */
