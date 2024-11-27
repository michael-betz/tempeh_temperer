#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H
#include <stdint.h>

extern uint8_t one_wire_error;

void init_one_wire(void);

// start conversion, wait 187.50 ms before calling read_temp()
void conv_temp();

// returns temperature in [degC] as signed fixed point number with nFract = 4
// returns 0 on CRC error
int16_t read_temp();

#endif
