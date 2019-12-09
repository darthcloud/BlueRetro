#ifndef _GC_H_
#define _GC_H_
#include "adapter.h"

void gc_meta_init(struct generic_ctrl *ctrl_data);
void gc_init_buffer(struct wired_data *wired_data);
void gc_from_generic(struct generic_ctrl *ctrl_data, struct wired_data *wired_data);

#endif /* _GC_H_ */
