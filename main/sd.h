#ifndef _SD_H_
#define _SD_H_

#include <esp_err.h>

struct btn {
    union {
        uint32_t val;
        struct {
            uint8_t btn0;
            uint8_t btn1;
            uint8_t reserved;
            uint8_t special;
        };
    };
} __packed;

struct config {
    uint32_t magic;
    uint8_t mode;
    uint8_t set_map;
    uint8_t reserved[2];
    struct btn mapping[4][32];
} __packed;

esp_err_t sd_init(struct config *config);
esp_err_t sd_update_config(struct config *config);
esp_err_t sd_update_mempak(void);

#endif /* _SD_H_ */

