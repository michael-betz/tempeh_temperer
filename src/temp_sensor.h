#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

// Maximum conversion time [ms]
#define CONV_TIME_MS 750

void init_one_wire(void);

// start conversion, wait 187.50 ms before calling read_temp()
void conv_temp();

// returns temperature in [degC] as signed fixed point number with nFract = 4
// returns 0 on CRC error
int16_t read_temp();

// returns true when new value is available and writes it to val
// val is a 8:4 signed fractional number (4 unused bits)
bool get_temp_meas(unsigned long ts_now, int16_t *val);

#endif
