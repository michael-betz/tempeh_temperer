#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H
#include <stdint.h>

#define SWAP_SENSORS

extern uint8_t one_wire_error;
extern uint8_t ds_addr_air[8];
extern uint8_t ds_addr_probe[8];

uint8_t init_one_wire(void);

// start conversion, wait before calling read_temp()
uint8_t conv_temp();

// writes temperature in [degC] as signed fixed point number with nFract = 4
// returns 0 on success
uint8_t read_temp(uint8_t *ds_addr, int16_t *val);

#endif
