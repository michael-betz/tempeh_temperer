#ifndef SSD1306_H
#define SSD1306_H

#include <stdbool.h>
#include <stdint.h>

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT  64
#define LV_BPP 1  // bits / pixel

void ssd_init();
void ssd_poweroff();
void ssd_poweron();
void ssd_contrast(uint8_t val);
void ssd_invert();  // swap on and off
void ssd_flip_x(bool val);  // swap left and right
void ssd_flip_y(bool val);  // swap up and down
void ssd_send();

// SET / GET a single pixel in the framebuffer
void setPixel(int16_t x, int16_t y, bool isSet);
bool getPixel(unsigned x, unsigned y);

// Set whole screen to fixed value
void fill(bool val);
void fillRect(int x0, int x1, int y0, int y1, bool isSet);
void writeFillRect(int x, int y, int w, int h, bool color);
void rect(int x0, int x1, int y0, int y1, bool isSet);
void writeRect(int x, int y, int w, int h, bool color);
void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void vLine(unsigned x, unsigned y, unsigned h, bool isSet);
void hLine(unsigned x, unsigned y, unsigned w, bool isSet);


#endif
