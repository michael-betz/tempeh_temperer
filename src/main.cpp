#include <avr/io.h>
#include <Arduino.h>
#include <EEPROM.h>
#include "i2cmaster.h"
#include "gfx.h"
#include "ssd1306.h"
#include "print.h"
#include "main.h"
#include "temp_sensor.h"

uint8_t one_wire_error = 0;
bool heater_enabled = true;

int32_t t_read = 0;
int32_t t_set = FP(35.0);
int32_t i_val = 0;
int32_t power = 0;

#define MAX_POWER (0xFF << FP_FRAC)
#define MIN_POWER (-0xFF << FP_FRAC)


void store(int32_t val, uint8_t slot)
{
	uint8_t sum = 0;
	for (uint8_t i=0; i<=3; i++) {
		EEPROM.write(i + (slot << 3), val & 0xFF);
		sum += val & 0xFF;
		val >>= 8;
	}
	EEPROM.write(4 + (slot << 3), sum);
	print_str("stored ");
	print_udec(slot);
	print_str("\n");
}

bool load(int32_t *val, uint8_t slot)
{
	int32_t tmp = 0;
	uint8_t sum = 0;
	for (uint8_t i=0; i<=3; i++) {
		uint8_t r = EEPROM.read((3 - i) + (slot << 3));
		sum += r;
		tmp <<= 8;
		tmp |= r;
	}
	if (EEPROM.read(4 + (slot << 3)) == sum) {
		*val = tmp;
		print_str("restored ");
		print_udec(slot);
		print_str("\n");
		return true;
	}
	print_str("EEPROM read ");
	print_udec(slot);
	print_str(" failed\n");
	return false;
}

// val is +- 255
static void set_heater(int16_t val)
{
	static bool last_sign = true;
	bool sign = val >= 0;

	// Set PWM value
	OCR2B = sign ? val : -val;

	if (sign != last_sign && val != 0) {
		// disable mosfet (disable PWM)
		TCCR2A &= ~(1 << COM2B1);
		delay(1);

		// switch relay
		digitalWrite(PIN_RELAY, !sign);
		delay(20);

		// enable PWM again
		TCCR2A |= (1 << COM2B1);
		last_sign = sign;
	}
}

void setup()
{
	// Init PWM timer
	digitalWrite(PIN_PWM, HIGH);
	pinMode(PIN_PWM, OUTPUT);
	digitalWrite(PIN_RELAY, LOW);
	pinMode(PIN_RELAY, OUTPUT);
	pinMode(PIN_UP, INPUT_PULLUP);
	pinMode(PIN_DOWN, INPUT_PULLUP);

	// Setup Timer2 for PWM controlling the N-mosfet
	TCCR2B = (TCCR2B & 0xF8) | 7;  // 6: 60 Hz, 7: 15 Hz
	OCR2B = 0;
	// Phase correct PWM (mode 1), Inverted output, Enabled
	TCCR2A = (1 << COM2B1) | (1 << COM2B0) | (1 << WGM20);
	TCCR2A |= (1 << COM2B1);

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

	init_one_wire();
	conv_temp();
	delay(CONV_TIME_MS);

	load(&t_set, 0);
	load(&i_val, 1);
}

static int32_t limit(int32_t val, int32_t a, int32_t b)
{
	return (val < a) ? a : (val > b) ? b : val;
}

void step()
{
	static const int32_t k_p = FP(70.0);
	static const int32_t k_i = FP(0.2);
	static const int32_t k_d = FP(0.0);  // limited due to sensors resolution

	static uint32_t cycle = 0;
	static int32_t p_val = 0;
	static int32_t d_val = 0;
	static int32_t r_ = 0, r__ = 0, r___ = 0;

	int32_t err = 0, diff = 0;
	if (cycle >= 3) {
		// Calculate error from 4 averaged temperature readings
		err = t_set - (t_read + r_ + r__ + r___ + 2) / 4;

		// Calculate differential from 4 readings apart
		diff = r___ - t_read;
	}

	// Squared error
	int32_t err2 = ((int64_t)err * err + FP_ROUND) >> FP_FRAC;
	if (err < 0) {
		err2 = -err2;
	}

	// Proportional term (from squared error)
	p_val = (err2 * k_p + FP_ROUND) >> FP_FRAC;
	p_val = limit(p_val, MIN_POWER - 1, MAX_POWER + 1);

	// Integral term (from linear error)
	if (abs(p_val) >= MAX_POWER) {
		// No point of integrating if we are pegged
		i_val = 0;
	} else {
		i_val += (err * k_i + FP_ROUND) >> FP_FRAC;
		i_val = limit(i_val, MIN_POWER, MAX_POWER);
	}

	// Differential term
	d_val = (diff * k_d + FP_ROUND) >> FP_FRAC;

	// Output sum with limiter
	power = limit(p_val + d_val + i_val, MIN_POWER, MAX_POWER);

	r___ = r__;
	r__ = r_;
	r_ = t_read;

	print_str("c ");
	print_udec(cycle);
	print_str(", ");

	print_str("s ");
	print_dec_fix(t_set, FP_FRAC, 2);
	print_str(", ");

	print_str("r ");
	print_dec_fix(t_read, FP_FRAC, 2);
	print_str(", ");

	print_str("p ");
	print_dec_fix(p_val, FP_FRAC, 2);
	print_str(", ");

	print_str("i ");
	print_dec_fix(i_val, FP_FRAC, 2);
	print_str(", ");

	print_str("d ");
	print_dec_fix(d_val, FP_FRAC, 2);
	print_str(", ");

	print_str("o: ");
	print_dec_fix(power, FP_FRAC, 2);
	print_str("\n");

	cycle++;

	if (cycle > 0 && (cycle % 200) == 0 && i_val != 0)
		store(i_val, 1);
}

void gui()
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
		gui();
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

void loop()
{
	int16_t temp;
	unsigned long ts_now = millis();

	if (get_temp_meas(ts_now, &temp)) {
		if (one_wire_error != 0) {
			power = 0;
			// TODO re-init freezes in ds.reset()  Why??
			init_one_wire();
		} else if (!heater_enabled) {
			t_read = temp << (FP_FRAC - 4);
			power = 0;
		} else {
			// New temperature value is available, run PID
			t_read = temp << (FP_FRAC - 4);
			step();
		}
		set_heater((power + FP_ROUND) >> FP_FRAC);

		// Here's a good place to do things which are blocking for a while
		gui();
	}

	buttons(ts_now);
	serial_in();

	// invert display every 1 h
	static unsigned long ts_last_invert = 0;
	if (ts_now - ts_last_invert > 1000l * 60 * 60) {
		ts_last_invert = ts_now;
		ssd_invert();
	}
}
