#include "ogx360.h"
#include <string.h>
#include "bluetooth/host.h"
#include "adapter/wireless/wireless.h"
#include "wired/ogx360_i2c.h" // Hack
#include <esp32/rom/ets_sys.h>

#define BIT(x) (1<<x)

#define BUTTON_MASK_SIZE 32
#define BUTTON_MASK_SIZE 32
#define NUM_DIGITAL_BUTTONS 8
#define NUM_ANALOG_BUTTONS 6
#define NUM_8BIT_AXIS 2
#define NUM_16BIT_AXIS 4

enum { // Digital buttons
    OGX360_D_UP,
    OGX360_D_DOWN,
    OGX360_D_LEFT,
    OGX360_D_RIGHT,
    OGX360_START,
    OGX360_BACK,
    OGX360_LSTICK,
    OGX360_RSTICK
};

enum { // Analog buttons
    OGX360_A,
    OGX360_B,
    OGX360_X,
    OGX360_Y,
    OGX360_BLACK,
    OGX360_WHITE,
};

static DRAM_ATTR const int ogx360_digital_btns_mask[BUTTON_MASK_SIZE] = {
    -1, -1, -1, -1,
    -1, -1, -1, -1,
    OGX360_D_LEFT, OGX360_D_RIGHT, OGX360_D_DOWN, OGX360_D_UP,
    -1, -1, -1, -1,
    -1, -1, -1, -1,
    OGX360_START, OGX360_BACK, -1, -1,
    -1, -1, -1, OGX360_LSTICK,
    -1, -1, -1, OGX360_RSTICK
};

static DRAM_ATTR const int ogx360_analog_btns_mask[BUTTON_MASK_SIZE] = {
    -1, -1, -1, -1,
    -1, -1, -1, -1,
    -1, -1, -1, -1,
    -1, -1, -1, -1,
    OGX360_X, OGX360_B, OGX360_A, OGX360_Y,
    -1, -1, -1, -1,
    -1, OGX360_BLACK, -1, -1,
    -1, OGX360_WHITE, -1, -1,
};

static DRAM_ATTR const struct ctrl_meta ogx360_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -32768, .size_max = 32767, .neutral = 0x00, .abs_max = 32767},
    {.size_min = -32768, .size_max = 32767, .neutral = 0x00, .abs_max = 32767},
    {.size_min = -32768, .size_max = 32767, .neutral = 0x00, .abs_max = 32767},
    {.size_min = -32768, .size_max = 32767, .neutral = 0x00, .abs_max = 32767},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 255},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 255},
};

static const uint32_t ogx360_mask[4] = {0xBBFF0FFF, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t ogx360_desc[4] = {0x110000FF, 0x00000000, 0x00000000, 0x00000000};

typedef struct __attribute__((packed)) usbd_duke_out
{
    uint8_t  controllerType; // 0xF1
    uint8_t  startByte;      // 0x00
    uint8_t  bLength;        // 0x06
    uint16_t wButtons;
    uint8_t  analogButtons[NUM_ANALOG_BUTTONS];
    uint8_t  axis8[NUM_8BIT_AXIS];
    uint16_t axis16[NUM_16BIT_AXIS];
} usbd_duke_out_t;

typedef struct __attribute__((packed)) usbd_duke_in
{
    uint8_t  startByte; // 0x00
    uint8_t  bLength;   // 0x06
    uint16_t lValue;    // Left Rumble
    uint16_t hValue;    // Right Rumble
} usbd_duke_in_t;

void ogx360_acc_toggle_fb(uint32_t wired_id, uint16_t left_motor, uint16_t right_motor);


void ogx360_meta_init(struct generic_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*4);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_PAD_ALT:
                    ctrl_data[i].mask = ogx360_mask;
                    ctrl_data[i].desc = ogx360_desc;
                    ctrl_data[i].axes[j].meta = &ogx360_axes_meta[j];
                    break;
                default:
                    ctrl_data[i].mask = ogx360_mask;
                    ctrl_data[i].desc = ogx360_desc;
                    ctrl_data[i].axes[j].meta = &ogx360_axes_meta[j];
                    break;
            }
        }
    }
}


void ogx360_from_generic(int32_t dev_mode, struct generic_ctrl *ctrl_data, struct wired_data *wired_data) {    
    struct usbd_duke_out *duke_out = (struct usbd_duke_out*) wired_data->output;
    struct usbd_duke_in *duke_in = (struct usbd_duke_in*) wired_data->output;
    
    //Start Rumble
    struct bt_data *bt_data = &bt_adapter.data[wired_data->index];
    if ( bt_data->pids->type == BT_XBOX) // Rumble is hanging PS4 controllers, untested on the rest.
    {
        if (duke_in->startByte == 0x00 && duke_in->bLength == 6)
        {
            ogx360_acc_toggle_fb(wired_data->index, duke_in->lValue, duke_in->hValue);        
        }
        else
        {
            ogx360_acc_toggle_fb(wired_data->index, 0, 0);
        }
    }
    
    //Start output
    duke_out->controllerType = 0xF1;
    duke_out->startByte = 0;
    duke_out->bLength = ( sizeof(struct usbd_duke_out) + 3 ) / 4; //Number of 4-byte blocks sent to Xbox
    
    duke_out->wButtons = 0;  // Digital buttons
    for (int i=0;i<BUTTON_MASK_SIZE;i++)
    {
        if (ogx360_digital_btns_mask[i] != -1)
        {
            if ((ctrl_data->btns[0].value & BIT(i)) != 0)
            {
                duke_out->wButtons |= BIT(ogx360_digital_btns_mask[i]);
            }
        }
    }
    
    for (int i=0;i<BUTTON_MASK_SIZE;i++) // Analog buttons
    {
        if (ogx360_analog_btns_mask[i] != -1)
        {
            bool pressed = (ctrl_data->btns[0].value & BIT(i)) > 0;
            if (pressed)
            {
                duke_out->analogButtons[ogx360_analog_btns_mask[i]] = 0xFF;
            }
            else
            {
                duke_out->analogButtons[ogx360_analog_btns_mask[i]] = 0x00;
            }
        }
    }
    
    for (int i=0;i<NUM_16BIT_AXIS;i++) // 16 bit axis
    {
        if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
            duke_out->axis16[i] = ctrl_data->axes[i].meta->size_max;
        }
        else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
            duke_out->axis16[i] = ctrl_data->axes[i].meta->size_min;
        }
        else {
            duke_out->axis16[i] = ctrl_data->axes[i].value;
        }
    }
    
    for (int i=0;i<NUM_8BIT_AXIS;i++) // 8 bit axis
    {
        uint8_t axes_index = NUM_16BIT_AXIS + i;
        if (ctrl_data->axes[axes_index].value > ctrl_data->axes[axes_index].meta->size_max) {
            duke_out->axis8[i] = ctrl_data->axes[axes_index].meta->size_max;
        }
        else if (ctrl_data->axes[axes_index].value < ctrl_data->axes[axes_index].meta->size_min) {
            duke_out->axis8[i] = ctrl_data->axes[axes_index].meta->size_min;
        }
        else {     
            duke_out->axis8[i] = ctrl_data->axes[axes_index].value;
        }
    }
    ogx360_process(wired_data->index);
}

void ogx360_acc_toggle_fb(uint32_t wired_id, uint16_t left_motor, uint16_t right_motor) {
    struct bt_dev *device = NULL;
    struct bt_data *bt_data = NULL;
    bt_host_get_dev_from_out_idx(wired_id, &device);
    if (device) {
        bt_data = &bt_adapter.data[device->ids.id];
        if (bt_data) {
            struct generic_fb fb_data = {0};
            fb_data.wired_id = wired_id;
            fb_data.type = FB_TYPE_RUMBLE;
            fb_data.cycles = 0;
            fb_data.start = 0;
            fb_data.state = (left_motor || right_motor);
            fb_data.left_motor = left_motor << 16 ;
            fb_data.right_motor = right_motor << 16;
            wireless_fb_from_generic(&fb_data, bt_data);
            bt_hid_feedback(device, bt_data->output);
        }
    }
}