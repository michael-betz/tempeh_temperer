#pragma once

extern int32_t t_set;
extern int32_t t_read;
extern int32_t power;
extern bool heater_enabled;

// Call this once
void pid_init();

// Call this with the cycle time
void pid_cycle();

// used EEPROM 32 bit slots
enum EE_SLOTS {
	SL_T_SET,
	SL_I_VAL,
	SL_PROCESS_0,
	SL_PROCESS_1
};

// Store 32 bit value into EEPROM
void store_ee(int32_t val, uint8_t slot);

// load 32 bit value from EEPROM, returns true on success
bool load_ee(int32_t *val, uint8_t slot);

// Limit a value to [a, b]
int32_t limit(int32_t val, int32_t a, int32_t b);


