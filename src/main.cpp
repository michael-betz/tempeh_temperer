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

#define MAX_HATCH 50

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

	// for (uint8_t i=0; i<N_STAGES; i++) {
	// 	uint32_t tmp = 0;
	// 	if (!load_ee((int32_t*)(&tmp), SL_PROCESS_0 + i)) {
	// 		tmp = (FP(38.0) << 16) | FP(32.0);
	// 		print_str("Slot "); print_hex(i, 1); print_str("invalid. Using 32 / 38 degC\n");
	// 	}
	// 	target_temperatures[i] = tmp & 0xFFFF;
	// 	max_temperatures[i] = (tmp >> 16) & 0xFFFF;
	// }
	if (!load_ee(&ms_since_start, SL_MS_SINCE_START)) {
		ms_since_start = 0;
		store_ee(ms_since_start, SL_MS_SINCE_START);
	}

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
				store_ee(t_set, SL_T_SET);
			}
			wp = 0;
			continue;
		}

		line_buf[wp++] = c;
		if (wp >= sizeof(line_buf))
			wp = 0;
	}
}

void open_hatch()
{
	// don't open hatch within first 1 h
	if (ms_since_start < 1000 * 60 * 60)
		return;

	if (t_read > t_set + FP(5.0))
		set_motor(1);
	else if (t_read < t_set)
		set_motor(-100);
}

void every_cycle(unsigned long ts_now)
{
	static unsigned cycle = 0;

	pid_cycle();
	open_hatch();

	// Here's a good place to do things which are blocking for a while
	gui(ts_now);

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
	serial_in();
}
