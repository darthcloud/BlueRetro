#ifndef _BT_H_
#define _BT_H_

#include "io.h"
#include "sd.h"

esp_err_t bt_init(struct io *io_data, struct config *config);

#endif /* _BT_H_ */

