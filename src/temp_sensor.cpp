#include<Arduino.h>
#include <OneWire.h>
#include "print.h"
#include "main.h"
#include "temp_sensor.h"

// a 4.7K resistor is necessary
OneWire ds(PIN_ONE_WIRE);

uint8_t ds_addr[8];

void init_one_wire(void)
{
	print_str("init_one_wire(): ");

	if (!ds.reset()) {  // TODO this freezes the second time
		print_str("not present\n");
		one_wire_error = 1;
		return;
	}

	if (!ds.search(ds_addr)) {
		print_str("search failed!\n");
		one_wire_error = 2;
		return;
	}
	hexDump(ds_addr, sizeof(ds_addr));

	if (OneWire::crc8(ds_addr, 7) != ds_addr[7]) {
		print_str("CRC invalid!\n");
		one_wire_error = 3;
		return;
	}

	// the first ROM byte is the chip-id
	switch (ds_addr[0]) {
		case 0x10:
			print_str(" DS18S20");  // or old DS1820
			break;
		case 0x28:
			print_str(" DS18B20");
			break;
		case 0x22:
			print_str(" DS1822");
			break;
		default:
			print_str("not a DS18x20 family device.\n");
			one_wire_error = 4;
			return;
	}
	print_str("\n");

	// Set configuration register
	if (!ds.reset()) {
		one_wire_error = 5;
		return;
	}
	ds.select(ds_addr);
	ds.write(0x4E);  // Write scratchpad
	ds.write(100);   // TH
	ds.write(-100);  // TL
	// 0x1F:  9 bit,  93.75 ms
	// 0x3F: 10 bit, 187.50 ms
	// 0x5F: 11 bit, 375.00 ms
	// 0x7F: 12 bit, 750.00 ms
	ds.write(0x7F);  // CFG

	one_wire_error = 0;
}

// start conversion, with parasite power on at the end
void conv_temp()
{
	if (!ds.reset()) {
		one_wire_error = 6;
		return;
	}
	ds.select(ds_addr);
	// ds.write(0xCC);  // Skip ROM search, for single devices on bus only
	ds.write(0x44, 1);  // Start conversion, now wait
}

int16_t read_temp()
{
	uint8_t data[9];

	if (!ds.reset()) {
		one_wire_error = 7;
		return 0;
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
		return 0;
	}

	one_wire_error = 0;
	return (data[1] << 8) | data[0];
}

// returns true when new value should available and writes it to val
// val is only valid if one_wire_error is 0
bool get_temp_meas(unsigned long ts_now, int16_t *val)
{
	static unsigned long ts_ready = 0;
	if (ts_now >= ts_ready) {
		*val = read_temp();
		conv_temp();
		ts_ready = ts_now + CONV_TIME_MS;
		return true;
	}
	return false;
}
