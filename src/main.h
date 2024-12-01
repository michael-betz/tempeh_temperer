#ifndef MAIN_H
#define MAIN_H

#define CYCLE_TIME 1000 // [ms]

// Number of fracional bits
// Need high resolution for integral-term to accumulate
// small differences with low (0.1) coefficients
#define FP_FRAC 6

#define FP_SCALE (1 << FP_FRAC)
#define FP_ROUND (1 << (FP_FRAC - 1))

// Convert floating point constant to fixed point
#define FP(v) ((int32_t)(v * FP_SCALE + 0.5))

#define MAX_POWER (0xFF << FP_FRAC)
#define MIN_POWER (0 << FP_FRAC)

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
