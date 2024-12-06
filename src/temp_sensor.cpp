#include<Arduino.h>
#include <OneWire.h>
#include "print.h"
#include "main.h"
#include "temp_sensor.h"

// a 4.7K resistor is necessary
OneWire ds(PIN_ONE_WIRE);

uint8_t n_sensors = 0;
uint8_t one_wire_error = 0;
uint8_t ds_addr_air[8];
uint8_t ds_addr_probe[8];

// returns 0 on success
static uint8_t init_sensor(uint8_t *ds_addr)
{
	if (!ds.search(ds_addr)) {
		return 2;
	}
	hexDump(ds_addr, 8);

	if (OneWire::crc8(ds_addr, 7) != ds_addr[7]) {
		return 3;
	}

	// the first ROM byte is the chip-id
	switch (ds_addr[0]) {
		case 0x10:
			print_str(" DS18S20\n");  // or old DS1820
			break;
		case 0x28:
			print_str(" DS18B20\n");
			break;
		case 0x22:
			print_str(" DS1822\n");
			break;
		default:
			return 4;
	}

	// Set configuration register
	if (!ds.reset())
		return 5;

	ds.select(ds_addr);
	ds.write(0x4E);  // Write scratchpad
	ds.write(100);   // TH
	ds.write(-100);  // TL
	// 0x1F:  9 bit,  93.75 ms
	// 0x3F: 10 bit, 187.50 ms
	// 0x5F: 11 bit, 375.00 ms
	// 0x7F: 12 bit, 750.00 ms
	ds.write(0x7F);  // CFG

	return 0;
}

// returns 0 on success
uint8_t init_one_wire(void)
{
	uint8_t ret = 0;

	n_sensors = 0;

	ds.reset_search();
	while (ds.search(ds_addr_air))
		n_sensors++;

	print_str("Number of 1-wire sensors found: ");
	print_dec(n_sensors);
	print_str("\n");

	if (n_sensors <= 0) {
		return 99;
	}

	if (n_sensors == 1) {
		ds.reset_search();
		return init_sensor(ds_addr_air);
	}

	// multiple sensors found
	n_sensors = 2;
	ds.reset_search();
	#ifdef SWAP_SENSORS
		ret = init_sensor(ds_addr_probe);
		ret |= init_sensor(ds_addr_air) << 4;
	#else
		ret = init_sensor(ds_addr_air);
		ret |= init_sensor(ds_addr_probe) << 4;
	#endif

	one_wire_error = ret;

	return ret;
}

// start conversion, with parasite power on at the end, return 0 on success
uint8_t conv_temp()
{
	if (!ds.reset()) {
		one_wire_error = 6;
		return one_wire_error;
	}

	ds.skip();			// address all sensors on the bus
	ds.write(0x44, 1);  // Start conversion, now wait
	one_wire_error = 0;
	return 0;
}

// return 0 on success
uint8_t read_temp(uint8_t *ds_addr, int16_t *val)
{
	uint8_t data[9];

	if (!ds.reset()) {
		one_wire_error = 7;
		return one_wire_error;
	}

	ds.select(ds_addr);
	ds.write(0xBE);  // Read Scratchpad
	ds.read_bytes(data, 9);

	uint8_t crc = OneWire::crc8(data, 8);
	if (data[8] != crc) {
		hexDump(data, 9);
		print_str("One-wire CRC Error. Expected: ");
		print_hex(crc, 2);
		print_str("\n");
		one_wire_error = 8;
		return one_wire_error;
	}

	if (val != NULL)
		*val = (data[1] << 8) | data[0];

	one_wire_error = 0;
	return one_wire_error;
}
