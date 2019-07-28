/* ESP32 RMT NSI Driver
 *
 * ESP32 demo app demonstrating sniffing Nintendo's serial interface via RMT peripheral.
 *
*/

#ifndef _NSI_H_
#define _NSI_H_

#include "io.h"

#define NSI_FRAME_MAX 63

typedef enum {
    NSI_CH_0 = 0,
    NSI_CH_1,
    NSI_CH_2,
    NSI_CH_3,
    NSI_CH_4,
    NSI_CH_5,
    NSI_CH_6,
    NSI_CH_7,
    NSI_CH_MAX
} nsi_channel_t;

typedef enum {
    NSI_MASTER = 0,
    NSI_SLAVE,
} nsi_mode_t;

typedef struct {
    uint8_t data[NSI_FRAME_MAX] __attribute__((aligned(8)));
    uint16_t len;
    uint8_t stop_len;
} nsi_frame_t;

void nsi_init(nsi_channel_t channel, uint8_t gpio, nsi_mode_t mode, struct io *output_data);
void nsi_reset(nsi_channel_t channel);
esp_err_t nsi_rx(nsi_channel_t channel, nsi_frame_t *frame);
void nsi_tx(nsi_channel_t channel, nsi_frame_t *frame);

#endif  /* _NSI_H_ */

