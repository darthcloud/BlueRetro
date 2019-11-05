#include "adapter.h"
#include "config.h"

static struct config config;

void config_init(void) {
    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < KB_MAX; j++) {
            config.map_conf[i][j].src_btn = j;
            config.map_conf[i][j].dst_btn = j;
            config.map_conf[i][j].dst_id = i;
            config.map_conf[i][j].turbo = 0;
            config.map_conf[i][j].algo = 0;
            config.map_conf[i][j].perc_max = 100;
            config.map_conf[i][j].perc_threshold = 25;
            config.map_conf[i][j].perc_deadzone = 10;
        }
    }
}
