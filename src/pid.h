#pragma once

// Number of fracional bits
// Need high resolution for integral-term to accumulate
// small differences with low (0.1) coefficients
#define FP_FRAC 6

#define FP_SCALE (1 << FP_FRAC)
#define FP_ROUND (1 << (FP_FRAC - 1))

// Convert floating point constant to fixed point
#define FP(v) ((int32_t)(v * FP_SCALE + 0.5))

// How many temperature values to average [cycles]
#define N_AVG 4

// ---------------------------------------------------------------
// Inner loop which controls the heater PWM from air temperature
// ---------------------------------------------------------------
// Used in dual sensor mode
#define AIR_KP_DUAL FP(200.0)  // PWM units / degC
#define AIR_KI_DUAL FP(0.0)  // PWM units / degC / s

// only used in single sensor mode
#define AIR_KP_SINGLE FP(150.0)
#define AIR_KI_SINGLE FP(0.5)

// PWM-value limits. Valid range from 0 to 0xFF
#define POWER_MAX_LIMIT (0xFF << FP_FRAC)
#define POWER_MIN_LIMIT (4 << FP_FRAC)  // not 0 to keep powerbank from shutting down

// ---------------------------------------------------------------
//  Outer loop which controls Tempeh probe temperature
// ---------------------------------------------------------------
#define PROBE_KP FP(10.0)  // degC / degC

// Air temperature set-point limits in [degC]
#define AIR_MAX_LIMIT FP(38.0)
#define AIR_MIN_LIMIT FP(20.0)

extern int16_t measured_air_temperature;
extern int16_t measured_probe_temperature;

extern int16_t target_probe_temperature;
extern int16_t target_air_temperature;
extern int16_t target_heater_power;

extern bool heater_enabled;

// Call this once
void pid_init();

// Call this with the cycle time
void pid_cycle();

// used EEPROM 32 bit slots
enum EE_SLOTS {
	SL_T_SET,
	SL_I_VAL,
	SL_MS_SINCE_START,
	SL_I_VAL_AIR
};

// Store 32 bit value into EEPROM
void store_ee(int32_t val, uint8_t slot);

// load 32 bit value from EEPROM, returns true on success
bool load_ee(int32_t *val, uint8_t slot);

// Limit a value to [a, b]
int32_t limit(int32_t val, int32_t a, int32_t b);


