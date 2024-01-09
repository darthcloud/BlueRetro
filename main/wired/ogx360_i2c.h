#ifndef _OGX360_I2C_H_
#define _OGX360_I2C_H_

void ogx360_check_connected_controllers();
void ogx360_initialize_i2c(void);
void ogx360_process(uint8_t player);

#endif /* _OGX360_I2C_H_ */
