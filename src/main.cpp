#include <avr/io.h>
#include <Arduino.h>
#include "i2cmaster.h"
#include "gfx.h"
#include "ssd1306.h"
#include "temp_sensor.h"
#include "print.h"
#include "main.h"
#include "pid.h"

void setup()
{
	// Init GPIOs timer
	digitalWrite(PIN_PWM, LOW);
	pinMode(PIN_PWM, OUTPUT);
	digitalWrite(PIN_RELAY, LOW);
	pinMode(PIN_RELAY, OUTPUT);
	pinMode(PIN_UP, INPUT_PULLUP);
	pinMode(PIN_DOWN, INPUT_PULLUP);

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
}

void gui(unsigned long ts_now)
{
	print_mux = PRINT_OLED;
	fill(0);

	if (heater_enabled) {
		// bar-graph
		hLine(0, 0, DISPLAY_WIDTH, true);
		hLine(0, 7, DISPLAY_WIDTH, true);
		int16_t p = (power + FP_ROUND) >> (FP_FRAC + 1);
		if (p > 0)
			fillRect(0, p, 2, 5, true);
		else if (p < 0)
			fillRect(DISPLAY_WIDTH + p - 1, DISPLAY_WIDTH - 1, 2, 5, true);
	} else {
		set_cursor(1, 1);
		print_str("Power disabled");
	}

	// temperature reading
	set_cursor(1, 17);
	set_size(4);
	if (one_wire_error > 0) {
		print_str("E");
		print_udec(one_wire_error);
	} else {
		print_dec_fix(t_read, FP_FRAC, 1);
		set_size(2);
		print_str(" C\n");
	}

	// set-point
	set_cursor(0, 57);
	set_size(1);
	print_str("set: ");
	print_dec_fix(t_set, FP_FRAC, 1);
	print_str(" C\n");

	ssd_send();
	print_mux = PRINT_UART;

	// invert display every 1 h
	static unsigned long ts_last_invert = 0;
	if (ts_now - ts_last_invert > 1000l * 60 * 60) {
		ts_last_invert = ts_now;
		ssd_invert();
	}
}

void buttons(unsigned long ts_now)
{
	static uint8_t idle_cycles=0xFF, pushed_cycles=0, n_incr=0;
	static unsigned long ts = 0;

	if (ts_now - ts <= 1)
		return;
	ts = ts_now;

	int8_t sign = 0;
	if (digitalRead(PIN_UP) == 0) {
		sign = 1;
	} else if (digitalRead(PIN_DOWN) == 0) {
		sign = -1;
	} else {
		pushed_cycles = 0;
		n_incr = 0;

		if (idle_cycles == 0xFE) {
			store(t_set, 0);
			if (!heater_enabled) {
				print_str("Enabling heater\n");
				heater_enabled = true;
			}
		}

		if (idle_cycles < 0xFF)
			idle_cycles++;

		return;
	}

	if (pushed_cycles == 5) {
		if (n_incr > 10) {
			t_set += FP(1.0) * sign;
		} else {
			t_set += FP(0.1) * sign;
			n_incr++;
		}
		t_set = limit(t_set, FP(0.0), FP(50.0));
		idle_cycles = 0;
		gui(ts_now);
	}

	if (pushed_cycles >= 50)
		pushed_cycles = 0;

	pushed_cycles++;
}

// For setting the temperature set-point over serial port
// for example send: 25.5\n to set it to 25.5 deg C
void serial_in()
{
	static char line_buf[16];
	static uint8_t wp = 0;

	while (Serial.available() > 0) {
		char c = Serial.read();

		if (c == '\r')
			return;

		if (c == '\n') {
			line_buf[wp] = '\0';
			String s = String(line_buf);
			if (s.equals("d")) {
				print_str("Disabling power\n");
				heater_enabled = false;
				wp = 0;
				continue;
			}
			if (s.equals("e")) {
				print_str("Enabling power\n");
				heater_enabled = true;
				wp = 0;
				continue;
			}
			float f = s.toFloat();
			if (f > 0 && f < 50.0) {
				print_str("New SP: ");
				print_str(line_buf);
				print_str("\n");
				t_set = f * FP_SCALE;
				store(t_set, 0);
			}
			wp = 0;
			continue;
		}

		line_buf[wp++] = c;
		if (wp >= sizeof(line_buf))
			wp = 0;
	}
}

void every_cycle(unsigned long ts_now)
{
	pid_cycle();

	// Here's a good place to do things which are blocking for a while
	gui(ts_now);
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
	serial_in();
}
