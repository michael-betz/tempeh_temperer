#include <stdint.h>
#include <stdlib.h>
#include <Arduino.h>
#include <EEPROM.h>
#include "pid.h"
#include "temp_sensor.h"
#include "print.h"
#include "main.h"


int16_t measured_air_temperature = 0;
int16_t measured_probe_temperature = 0;

int16_t target_heater_power = 0;
int16_t target_air_temperature = 0;
int16_t target_probe_temperature = 0;

bool heater_enabled = false;

static int32_t probe_i_val = 0;
static int32_t air_i_val = 0;

// val is 0 ... 255
static void set_heater(int16_t val)
{
	if (!heater_enabled) {
		OCR2B = 0;
		return;
	}

	val = (val + FP_ROUND) >> FP_FRAC;

	if (val < 0)
		val = 0;

	if (val > 0xFF)
		val = 0xFF;

	OCR2B = val;
}

// Sets target heater power
void pid_air_step()
{
	const int32_t air_kp = (n_sensors >= 2) ? AIR_KP_DUAL : AIR_KP_SINGLE;
	const int32_t air_ki = (n_sensors >= 2) ? AIR_KI_DUAL : AIR_KI_SINGLE;

	// Calculate error term
	int32_t err = target_air_temperature - measured_air_temperature;

	// Proportional term, output sum with limiter
	int32_t p_val = (err * air_kp + FP_ROUND) >> FP_FRAC;

	// Integral term (from linear error)
	if (abs(err) > FP(1.5)) {
		// Leave at half power if we are further than 1.5 C away from target
		air_i_val = (POWER_MIN_LIMIT + POWER_MAX_LIMIT) / 2;
	} else {
		air_i_val += (err * air_ki + FP_ROUND) >> FP_FRAC;
		air_i_val = limit(air_i_val, POWER_MIN_LIMIT, POWER_MAX_LIMIT);
	}

	target_heater_power = limit(p_val + air_i_val, POWER_MIN_LIMIT, POWER_MAX_LIMIT);
}

// Returns target air temperature
void pid_probe_step()
{
	// Calculate error term
	int32_t err = target_probe_temperature - measured_probe_temperature;

	// Proportional term
	int32_t p_val = (err * PROBE_KP + FP_ROUND) >> FP_FRAC;
	// p_val = limit(p_val, AIR_MIN_LIMIT - 1, AIR_MAX_LIMIT + 1);

	// Integral term (from linear error)
	if (abs(err) > FP(0.3)) {
		// Reset the I-part if we are pegged
		probe_i_val = target_probe_temperature * 8;
	} else {
		// move I-term with the minimum increment possible
		probe_i_val += err > 0 ? 1 : -1;  // (err * PROBE_KI + FP_ROUND) >> FP_FRAC;
		probe_i_val = limit(probe_i_val, AIR_MIN_LIMIT * 8, AIR_MAX_LIMIT * 8);
	}

	print_str("pi ");
	print_dec_fix(probe_i_val, FP_FRAC, 2);
	print_str(", ");

	print_str("pp ");
	print_dec_fix(p_val, FP_FRAC, 2);
	print_str(", ");

	// Output sum with limiter
	target_air_temperature = limit(p_val + probe_i_val / 8, AIR_MIN_LIMIT, AIR_MAX_LIMIT);
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
	if (init_one_wire() != 0) {
		print_str("Sensor error, disabling heater\n");
		heater_enabled = false;
	} else {
		heater_enabled = true;
	}

	conv_temp();

	int32_t tmp_val = 0;

	load_ee(&tmp_val, SL_T_SET);
	target_probe_temperature = tmp_val;

	load_ee(&probe_i_val, SL_I_VAL);

	// Make sure a valid temp. readin is available in the first cycle
	delay(CYCLE_TIME);
}

static int16_t get_avg_temp(int16_t reading, int16_t *old_readings)
{
	int16_t sum = 0;

	// Generate the sum
	for (uint8_t i=0; i<(N_AVG - 1); i++)
		sum += old_readings[i];
	sum += reading;

	// Shift values towards higher indices
	for (uint8_t i=(N_AVG - 2); i>0; i--)
		old_readings[i] = old_readings[i - 1];

	old_readings[0] = reading;

	return (sum << (FP_FRAC - 4)) / N_AVG;
}

// Call this with the cycle time
void pid_cycle()
{
	static uint32_t cycle = 0;
	static int16_t temperature_air[N_AVG - 1];
	static int16_t temperature_probe[N_AVG - 1];

	// Read the two one wire temp. sensors
	int16_t tmp_air = 0, tmp_probe = 0;
	uint8_t ret = read_temp(ds_addr_air, &tmp_air);

	if (n_sensors >= 2)
		ret |= read_temp(ds_addr_probe, &tmp_probe) << 4;

	// Start the next temperature conversion already
	ret |= conv_temp();

	if (ret != 0) {
		heater_enabled = false;
		set_heater(0);

		print_str("one wire error "); print_dec(ret); print_str("\n");

		// TODO re-init freezes in ds.reset()  Why??
		init_one_wire();
		return;
	}

	// New temperature values are available
	measured_air_temperature = get_avg_temp(tmp_air, temperature_air);
	if (n_sensors >= 2)
		measured_probe_temperature = get_avg_temp(tmp_probe, temperature_probe);

	// Wait for averaging to converge
	if (cycle < N_AVG) {
		cycle++;
		return;
	}

	print_str("a ");
	print_dec_fix(measured_air_temperature, FP_FRAC, 2);
	print_str(" / ");
	print_dec_fix(target_air_temperature, FP_FRAC, 2);
	print_str(", ");

	print_str("p ");
	print_dec_fix(measured_probe_temperature, FP_FRAC, 2);
	print_str(" / ");
	print_dec_fix(target_probe_temperature, FP_FRAC, 2);
	print_str(", ");

	if (n_sensors >= 2)
		pid_probe_step();
	else
		target_air_temperature = target_probe_temperature;

	pid_air_step();
	set_heater(target_heater_power);

	print_str("h ");
	if (heater_enabled)
		print_dec_fix(target_heater_power, FP_FRAC, 2);
	else
		print_str("off");
	print_str("\n");

	if ((cycle % 600) == 0) {
		if (probe_i_val != 0)
			store_ee(probe_i_val, SL_I_VAL);
		if (air_i_val != 0)
			store_ee(air_i_val, SL_I_VAL_AIR);
	}

	cycle++;
}

void store_ee(int32_t val, uint8_t slot)
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

bool load_ee(int32_t *val, uint8_t slot)
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
