/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 No0ne (https://github.com/No0ne)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "ps2pico.h"
#include "pico/stdlib.h"
#include "xtphy.pio.h"

u32 const repeat_us = 91743;
u16 const delay_ms = 500;

u8 leds = 0;
u8 repeat = 0;
u8 last_pc;
u8 stuck;

bool blinking = false;

alarm_id_t repeater;
alarm_id_t resetter;

u8 const mod2xt[] = { 0x1d, 0x2a, 0x38, 0x5b, 0x1d, 0x36, 0x38, 0x5c };
u8 const hid2xt[] = {
  0x00, 0xff, 0xfc, 0x00, 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26,
  0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c, 0x02, 0x03,
  0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x1c, 0x01, 0x0e, 0x0f, 0x39, 0x0c, 0x0d, 0x1a,
  0x1b, 0x2b, 0x2b, 0x27, 0x28, 0x29, 0x33, 0x34, 0x35, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
  0x41, 0x42, 0x43, 0x44, 0x57, 0x58, 0x37, 0x46, 0x46, 0x52, 0x47, 0x49, 0x53, 0x4f, 0x51, 0x4d,
  0x4b, 0x50, 0x48, 0x45, 0x35, 0x37, 0x4a, 0x4e, 0x1c, 0x4f, 0x50, 0x51, 0x4b, 0x4c, 0x4d, 0x47,
  0x48, 0x49, 0x52, 0x53, 0x56, 0x5d, 0x5e, 0x59, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
  0x6c, 0x6d, 0x6e, 0x76
};

void xt_send(uint8_t data) {
  pio_sm_put(pio0, 0, (data ^ 0xff) << 1);
}

void xt_maybe_send_e0(u8 key) {
  if(key == 0x46 ||
    (key >= 0x49 && key <= 0x52) ||
     key == 0x54 || key == 0x58 ||
     key == 0x65 || key == 0x66 ||
    (key > 0xe2 && key != 0xe5)) {
    xt_send(0xe0);
  }
}

void xt_set_led(u8 led) {
  leds ^= led;
  tuh_kb_set_leds(leds);
}

int64_t repeat_callback(alarm_id_t id, void *user_data) {
  if(repeat) {
    if(repeat >= 0xe0 && repeat <= 0xe7) {
      xt_send(mod2xt[repeat - 0xe0]);
    } else {
      xt_send(hid2xt[repeat]);
    }
    
    return repeat_us;
  }
  
  repeater = 0;
  return 0;
}

void kb_send_key(u8 key, bool state, u8 modifiers) {
  if(key > 0x73 && key < 0xe0 && key > 0xe7) return;
  
  if(state && key == 0x53) xt_set_led(1);
  if(state && key == 0x39) xt_set_led(2);
  if(state && key == 0x47) xt_set_led(4);
  
  if(key == 0x48) {
    repeat = 0;
    
    if(state) {
      if(modifiers & 0x1 || modifiers & 0x10) {
        xt_send(0xe0); xt_send(0x46);
        xt_send(0xe0); xt_send(0xc6);
      } else {
        xt_send(0xe1); xt_send(0x1d); xt_send(0x45);
        xt_send(0xe1); xt_send(0x9d); xt_send(0xc5);
      }
    }
    
    return;
  }
  
  if(state) {
    repeat = key;
    if(repeater) cancel_alarm(repeater);
    repeater = add_alarm_in_ms(delay_ms, repeat_callback, NULL, false);
  } else {
    if(key == repeat) repeat = 0;
  }
  
  xt_maybe_send_e0(key);
  
  if(key >= 0xe0 && key <= 0xe7) {
    key = mod2xt[key - 0xe0];
  } else {
    key = hid2xt[key];
  }
  
  if(state) {
    xt_send(key);
  } else {
    xt_send(key | 0x80);
  }
}

int64_t blink_callback(alarm_id_t id, void *user_data) {
  if(blinking) {
    tuh_kb_set_leds(7);
    blinking = false;
    return 500000;
  }
  
  tuh_kb_set_leds(leds);
  return 0;
}

int64_t kb_reset() {
  xt_send(0xaa);
  leds = 0;
  repeat = 0;
  blinking = true;
  add_alarm_in_ms(50, blink_callback, NULL, false);
  return 0;
}

int64_t reset_detect() {
  u8 pc = pio_sm_get_pc(pio0, 0);
  stuck = last_pc == pc ? stuck + 1 : 0;
  last_pc = pc;
  
  if(stuck == 5) {
    stuck = 0;
    pio_sm_drain_tx_fifo(pio0, 0);
    if(resetter) cancel_alarm(resetter);
    resetter = add_alarm_in_ms(50, kb_reset, NULL, false);
  }
  
  return 5000;
}

void kb_init() {
  xtphy_program_init(pio0, 0, pio_add_program(pio0, &xtphy_program));
  add_alarm_in_ms(1000, reset_detect, NULL, false);
}
