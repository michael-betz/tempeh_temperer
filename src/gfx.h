#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include <stdlib.h>

void set_cursor(uint8_t x, uint8_t y);
void set_size(uint8_t s);

// printf to display
void pd(const char *format, ...);
// void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t size);

void gui(unsigned long ts_now);
void buttons(unsigned long ts_now);

extern uint8_t print_mux;
#define PRINT_OLED (1 << 0)
#define PRINT_UART (1 << 1)

#endif
