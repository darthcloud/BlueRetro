#include <esp32/rom/ets_sys.h>
#include "adapter/adapter.h"
#include "driver/i2c.h"
#include <soc/i2c_periph.h>
#include "ogx360_i2c.h"
#include <string.h>

bool playerConnected[4] = {0};

void ogx360_check_connected_controllers()
{
    ets_printf("OGX360_I2C: Looking for attached OGX360 modules\n");
    for (int i=0;i<4;i++)
    {
        const char ping[] = { 0xAA };
        esp_err_t result = i2c_master_write_to_device(I2C_NUM_0, i + 1, (void*)ping, sizeof(ping), 150);
        playerConnected[i] = (result == ESP_OK);
        ets_printf("OGX360_I2C: Player %d %s\n", i, playerConnected[i] ? "Found" : "Not Found");
    }
}

void ogx360_initialize_i2c(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = 21,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
        .clk_flags = 0,
    };
    ets_printf("OGX360_I2C: Init I2C\n");
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    I2C0.timeout.tout = 1048575;
    i2c_set_timeout(I2C_NUM_0, 1048575);
}

bool initialized = false;

void ogx360_process(uint8_t player)
{
    if (!initialized)
    {
        ogx360_initialize_i2c();
        ogx360_check_connected_controllers();
        initialized = true;
    }

//  while(1) // Hack
    {
        //for (int player=0;player<4;player++)
        {
            if (playerConnected[player])
            {
                esp_err_t result = i2c_master_write_to_device(I2C_NUM_0, player + 1, (void*)&wired_adapter.data[player].output, 21, 1);
                if (result != ESP_OK) ets_printf("OGX360 write result: %d Index: %d\n",result,player);
                if (result == 0)
                {    
                    memset((void*)&wired_adapter.data[player].output,0,7);
                    result = i2c_master_read_from_device(I2C_NUM_0,player + 1, (void*)&wired_adapter.data[player].output, 7, 1048575);
                    if (result != ESP_OK) ets_printf("OGX360 read result: %d Index: %d\n",result,player);
                }
            }
        }
    }
}