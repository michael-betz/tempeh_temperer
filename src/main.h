#ifndef MAIN_H
#define MAIN_H

#define CYCLE_TIME 1000 // [ms]

// PWM pins: 3, 5, 6, 9, 10, 11
// Heater PWM pin
#define PIN_PWM 3

// Motor to open / close the lid
#define PIN_MOTOR_DIRECTION 12
#define PIN_MOTOR_ENABLE 10

#define PIN_UP 8
#define PIN_DOWN 2
#define PIN_MID 7

#define PIN_ONE_WIRE 9

extern uint32_t ms_since_start;

#endif
