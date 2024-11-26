#ifndef MAIN_H
#define MAIN_H

// Number of fracional bits
// Need high resolution for integral-term to accumulate
// small differences with low (0.1) coefficients
#define FP_FRAC 6

#define FP_SCALE (1 << FP_FRAC)
#define FP_ROUND (1 << (FP_FRAC - 1))

// Convert floating point constant to fixed point
#define FP(v) ((int32_t)(v * FP_SCALE + 0.5))

// PWM pins: 3, 5, 6, 9, 10, 11
#define PIN_PWM 3
#define SET_HEATER(x) { analogWrite(PIN_PWM, 0xFF - (x)); }

#define PIN_RELAY 12

#define PIN_UP 8
#define PIN_DOWN 2

#define PIN_ONE_WIRE 9

extern uint8_t one_wire_error;

#endif
