// ripped from https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Adafruit_GFX.cpp

#include "gfx.h"
#include "glcdfont.cpp"
#include "ssd1306.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include<Arduino.h>

int16_t cursor_x = 0;     ///< x location to start print()ing text
int16_t cursor_y = 0;     ///< y location to start print()ing text
uint8_t textsize = 1;   ///< Desired magnification of text to print()

// Draw a character
/**************************************************************************/
/*!
   @brief   Draw a single character
    @param    x   Bottom left corner x coordinate
    @param    y   Bottom left corner y coordinate
    @param    c   The 8-bit font-indexed character (likely ascii)
    @param    size  Font magnification level, 1 is 'original' size
*/
/**************************************************************************/
static void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t size) {
  for (int8_t i = 0; i < 5; i++) { // Char bitmap = 5 columns
    uint8_t line = pgm_read_byte(&font[c * 5 + i]);
    for (int8_t j = 0; j < 8; j++, line >>= 1) {
      if (line & 1) {
        if (size == 1)
          setPixel(x + i, y + j, 1);
        else
          writeFillRect(x + i * size, y + j * size, size, size, 1);
      }
    }
  }
}

/**************************************************************************/
/*!
    @brief  Print one byte/character of data, used to support print()
    @param  c  The 8-bit ascii character to write
*/
/**************************************************************************/
static void write(uint8_t c) {
  // Ignore carriage returns
  if (c == '\r')
    return;

  // New-lines
  if (c == '\n') {
    cursor_x = 0;
    cursor_y += textsize * 8; // advance y one line
    return;
  }

  // form-feed = clear screen
  if (c == '\f') {
    fill(0);
    cursor_x = 0;
    cursor_y = 0;
    return;
  }

  // line-breaks
  // if ((cursor_x + textsize * 6) >= DISPLAY_WIDTH - 1) {
  //   cursor_x = 0;
  //   cursor_y += textsize * 8; // advance y one line
  // }

  drawChar(cursor_x, cursor_y, c, textsize);
  cursor_x += textsize * 6;
}

void set_cursor(uint8_t x, uint8_t y)
{
  cursor_x = x;
  cursor_y = y;
}

void set_size(uint8_t s)
{
  textsize = s;
}

uint8_t print_mux = PRINT_UART;

// to make print.h work
void _putchar(char c) {
  if (print_mux & PRINT_OLED)
    write(c);
  if (print_mux & PRINT_UART)
    Serial.write(c);
}
