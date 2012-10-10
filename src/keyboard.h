#pragma once
#include "midi.h"

// keymap enum callback
struct keymap_enum_callback {
  virtual void operator () (const char *value) = 0;
};

// initialize keyboard
int keyboard_init();

// shutdown keyboard system
void keyboard_shutdown();

// enable or disable keyboard
void keyboard_enable(bool enable);

// enum keymap
void keyboard_enum_keymap(keymap_enum_callback &callback);

// reset keyboard
void keyboard_reset();

// key event
void keyboard_key_event(int code, int keydown);

// keyboard event
void keyboard_update(double time_elapsed);