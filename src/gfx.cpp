// ripped from https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Adafruit_GFX.cpp

#include "gfx.h"
#include "pid.h"
#include "main.h"
#include "print.h"
#include "temp_sensor.h"
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


// void draw_top()
// {
//   writeFillRect(0, 0, 26, 6, 0);

//   for (uint8_t i=0; i<=N_STAGES; i++) {
//     if (current_stage == i)
//       writeFillRect(2 + i * 8, 0, 6, 6, 1);
//     else
//       writeRect(2 + i * 8, 0, 6, 6, 1);
//   }
// }


void gui(unsigned long ts_now)
{
  print_mux = PRINT_OLED;
  fill(0);

  // ----------------------
  //  Top row (yellow)
  // ----------------------
  if (heater_enabled) {
    // bar-graph on the top left
    hLine(0, 0, DISPLAY_WIDTH / 2, true);
    hLine(0, 7, DISPLAY_WIDTH / 2, true);
    int16_t p = (target_heater_power + FP_ROUND) >> (FP_FRAC + 2);
    if (p > 0)
      fillRect(0, p, 2, 5, true);
  } else {
    set_cursor(1, 1);
    print_str("disabled");
  }

  // process run time on the top right
  set_cursor(DISPLAY_WIDTH / 2, 8);
  set_size(1);

  uint32_t t_process_secs = ms_since_start / 1000;
  uint16_t t_process_mins = t_process_secs / 60;

  print_dec(t_process_mins / 60);
  print_str(":");
  print_udec_dp(t_process_mins % 60, 2, 0);
  print_str(":");
  print_udec_dp(t_process_secs % 60, 2, 0);

  // ----------------------
  //  temperature reading
  // ----------------------
  set_cursor(0, 17);
  set_size(3);
  if (one_wire_error > 0) {
    print_str("E");
    print_udec(one_wire_error);
  } else {
    print_dec_fix(measured_air_temperature, FP_FRAC, 1);
    if (n_sensors >= 2) {
      set_cursor(DISPLAY_WIDTH / 2, 17);
      print_dec_fix(measured_probe_temperature, FP_FRAC, 1);
    }
  }

  // ----------------------
  //  set-points
  // ----------------------
  set_size(1);
  set_cursor(0, 48);
  print_str("air\n");
  print_dec_fix(target_air_temperature, FP_FRAC, 1);
  print_str(" C  ");

  if (n_sensors >= 2) {
    set_cursor(DISPLAY_WIDTH / 2, 48);
    print_str("probe\n");
    print_dec_fix(target_probe_temperature, FP_FRAC, 1);
    print_str(" C  ");
  }

  ssd_send();
  print_mux = PRINT_UART;
}

void buttons(unsigned long ts_now)
{
  static uint8_t idle_cycles=0xFF, pushed_cycles=0, n_incr=0;
  static uint16_t mid_cycles = 0;
  static unsigned long ts = 0;

  if (ts_now - ts <= 1)
    return;
  ts = ts_now;

  if (digitalRead(PIN_MID) == 0) {
    mid_cycles++;
    if (mid_cycles >= 500) {
      print_str("reseting process timer\n");
      mid_cycles = 0;
      ms_since_start = 0;
    }
  } else {
    mid_cycles = 0;
  }

  int8_t sign = 0;
  if (digitalRead(PIN_UP) == 0) {
    sign = 1;
  } else if (digitalRead(PIN_DOWN) == 0) {
    sign = -1;
  } else {
    pushed_cycles = 0;
    n_incr = 0;

    if (idle_cycles == 0xFE) {
      store_ee(target_probe_temperature, SL_T_SET);
      if (!heater_enabled) {
        print_str("Enabling heater\n");
        heater_enabled = true;
      }
    }

    if (idle_cycles < 0xFF)
      idle_cycles++;

    return;
  }

  if (pushed_cycles == 5) {
    if (n_incr > 10) {
      target_probe_temperature += FP(1.0) * sign;
    } else {
      target_probe_temperature += FP(0.1) * sign;
      n_incr++;
    }
    target_probe_temperature = limit(target_probe_temperature, AIR_MIN_LIMIT, AIR_MAX_LIMIT);
    idle_cycles = 0;
    gui(ts_now);
  }

  if (pushed_cycles >= 50)
    pushed_cycles = 0;

  pushed_cycles++;
}
