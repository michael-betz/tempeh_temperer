#include <avr/io.h>
#include <Arduino.h>
#include "i2cmaster.h"
#include "gfx.h"
#include "ssd1306.h"
#include "temp_sensor.h"
#include "print.h"
#include "main.h"
#include "pid.h"

// process time
uint32_t ms_since_start = 0;

#define MAX_HATCH 55

void set_motor(int8_t amount)
{
	static int16_t current_pos = 0;

	int16_t target = current_pos + amount;
	if (target < 0)
		target = 0;
	else if (target > MAX_HATCH)
		target = MAX_HATCH;

	amount = target - current_pos;
	if (amount == 0) {
		return;
	} else if (amount < 0) {
		digitalWrite(PIN_MOTOR_DIRECTION, HIGH);
		delay(100);
		amount = -amount;
	}

	for (uint8_t i=0; i<amount; i++) {
		digitalWrite(PIN_MOTOR_ENABLE, HIGH);
		delay(250);
	}

	digitalWrite(PIN_MOTOR_ENABLE, LOW);
	digitalWrite(PIN_MOTOR_DIRECTION, LOW);
	current_pos = target;
	print_str("Hatch @ "); print_dec(current_pos); print_str("\n");
}


void setup()
{
	// Init GPIOs timer
	digitalWrite(PIN_PWM, LOW);
	pinMode(PIN_PWM, OUTPUT);

	digitalWrite(PIN_MOTOR_DIRECTION, LOW);
	pinMode(PIN_MOTOR_DIRECTION, OUTPUT);

	digitalWrite(PIN_MOTOR_ENABLE, LOW);
	pinMode(PIN_MOTOR_ENABLE, OUTPUT);

	pinMode(PIN_UP, INPUT_PULLUP);
	pinMode(PIN_DOWN, INPUT_PULLUP);
	pinMode(PIN_MID, INPUT_PULLUP);

	Serial.begin(115200);
	print_str("Yo! This is Tempeh Temperer!\n");

	i2c_init();

	print_str("I2C: ");
	for (unsigned i=0; i<127; i++) {
		uint8_t ret = i2c_start(i << 1);
		i2c_stop();
		if (ret == 0) {
			print_hex(i, 2); print_str(" ");
		}
	}
	print_str("\n");

	ssd_init();
	// Set random display inverted state on power-up
	if (analogRead(0) & 1)
		ssd_invert();

	pid_init();

	if (!load_ee((int32_t*)(&ms_since_start), SL_MS_SINCE_START)) {
		ms_since_start = 0;
		store_ee(ms_since_start, SL_MS_SINCE_START);
	}
}

void open_hatch()
{
	// don't open hatch within first 1 h
	if (ms_since_start < (1000L * 60 * 60))
		return;

	if (measured_probe_temperature > target_probe_temperature + FP(3.0))
		set_motor(1);
	else if (measured_probe_temperature < target_probe_temperature)
		set_motor(-100);
}

void every_cycle(unsigned long ts_now)
{
	static unsigned cycle = 0;

	pid_cycle();
	open_hatch();

	// Here's a good place to do things which are blocking for a while
	gui(ts_now);

  	// save some values to EEPROM every 10 min
	if (cycle > 0 && (cycle % 600) == 0) {
		store_ee(ms_since_start, SL_MS_SINCE_START);
	}

	// invert display every 1 h
	if (cycle > 0 && (cycle % 3600) == 0)
	    ssd_invert();

	// keep track of process time
	static unsigned long last_ms = 0;
	ms_since_start += ts_now - last_ms;
	last_ms = ts_now;

	cycle++;
}

void loop()
{
	static unsigned long ts_next = 0;

	unsigned long ts_now = millis();

	if (ts_now >= ts_next) {
		ts_next = ts_now + CYCLE_TIME;
		// called at 1 Hz
		every_cycle(ts_now);
	}

	// Called as fast as possible
	buttons(ts_now);
}
