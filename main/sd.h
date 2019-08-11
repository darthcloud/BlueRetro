#ifndef _SD_H_
#define _SD_H_

#include <esp_err.h>

struct config {
    uint32_t magic;
    uint8_t mode;
    uint8_t set_map;
    uint8_t reserved[2];
    uint8_t mapping[4][32];
} __packed;

esp_err_t sd_init(struct config *config);
esp_err_t sd_update_config(struct config *config);

#endif /* _SD_H_ */

