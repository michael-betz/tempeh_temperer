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
	// Calculate error term
	int32_t err = target_air_temperature - measured_air_temperature;

	// Proportional term, output sum with limiter
	int32_t p_val = (err * AIR_KP + FP_ROUND) >> FP_FRAC;
	target_heater_power = limit(p_val, POWER_MIN_LIMIT, POWER_MAX_LIMIT);
}

// Returns target air temperature
void pid_probe_step()
{
	// Calculate error term
	int32_t err = target_probe_temperature - measured_probe_temperature;

	// Proportional term
	int32_t p_val = (err * PROBE_KP + FP_ROUND) >> FP_FRAC;
	p_val = limit(p_val, AIR_MIN_LIMIT - 1, AIR_MAX_LIMIT + 1);

	// Integral term (from linear error)
	if (p_val <= AIR_MIN_LIMIT || p_val >= AIR_MAX_LIMIT) {
		// Center the I-part if we are pegged
		probe_i_val = (AIR_MIN_LIMIT + AIR_MAX_LIMIT) / 2;
	} else {
		probe_i_val += (err * PROBE_KI + FP_ROUND) >> FP_FRAC;
		probe_i_val = limit(probe_i_val, AIR_MIN_LIMIT, AIR_MAX_LIMIT);
	}

	// Output sum with limiter
	target_air_temperature = limit(p_val + probe_i_val, AIR_MIN_LIMIT, AIR_MAX_LIMIT);
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

	return (sum / N_AVG) << (FP_FRAC - 4);
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
	measured_probe_temperature = get_avg_temp(tmp_probe, temperature_probe);

	// Wait for averaging to converge
	if (cycle < N_AVG) {
		cycle++;
		return;
	}

	pid_probe_step();
	pid_air_step();
	set_heater(target_heater_power);

	if ((cycle % 600) == 0 && probe_i_val != 0)
		store_ee(probe_i_val, SL_I_VAL);

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
