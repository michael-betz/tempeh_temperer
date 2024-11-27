#include <stdint.h>
#include <stdlib.h>
#include <Arduino.h>
#include <EEPROM.h>
#include "pid.h"
#include "temp_sensor.h"
#include "print.h"
#include "main.h"

int32_t t_read = 0;
int32_t t_set = FP(35.0);
int32_t power = 0;
bool heater_enabled = true;

static int32_t i_val = 0;

// val is 0 ... 255
static void set_heater(uint8_t val)
{
	if (heater_enabled)
		OCR2B = val;
	else
		OCR2B = 0;
}

static void pid_step()
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

	if (!heater_enabled)
		power = 0;

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

int32_t limit(int32_t val, int32_t a, int32_t b)
{
	return (val < a) ? a : (val > b) ? b : val;
}

void pid_init()
{
	// Setup Timer2 for PWM controlling the N-mosfet driving the heater
	TCCR2B = (TCCR2B & 0xF8) | 7;  // 6: 60 Hz, 7: 15 Hz
	OCR2B = 0;
	// Phase correct PWM (mode 1), Non-inverted output, Enabled
	TCCR2A = (1 << COM2B1) | (0 << COM2B0) | (1 << WGM20);
	TCCR2A |= (1 << COM2B1);

	// Init one wire interface to temperature sensor
	init_one_wire();
	conv_temp();

	load(&t_set, 0);
	load(&i_val, 1);

	// Make sure a valid temp. readin is available in the first cycle
	delay(CYCLE_TIME);
}

// Call this with the cycle time
void pid_cycle()
{
	// Read the one wire temp. sensors
	int16_t tmp = read_temp();

	// Start the next temperature conversion already
	conv_temp();

	if (one_wire_error == 0) {
		// New temperature value is available, run PID
		t_read = tmp << (FP_FRAC - 4);
		pid_step();
	} else {
		power = 0;
		// TODO re-init freezes in ds.reset()  Why??
		init_one_wire();
	}
	set_heater((power + FP_ROUND) >> FP_FRAC);
}

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
