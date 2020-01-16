#ifndef _DC_H_
#define _DC_H_
#include "adapter.h"

void dc_meta_init(struct generic_ctrl *ctrl_data);
void dc_init_buffer(struct wired_data *wired_data);
void dc_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data);
void dc_fb_to_generic(uint8_t *raw_fb_data, uint32_t raw_fb_len, struct generic_fb *fb_data);

#endif /* _DC_H_ */
