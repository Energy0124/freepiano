#pragma once

struct midi_enum_callback {
  virtual void operator () (const char *value) = 0;
};

// open output device
int midi_open_output(const char *name);

// close output device
void midi_close_output();

// open input device
void midi_open_inputs();

// close input device
void midi_close_inputs();

// send event
void midi_send_event(byte a, byte b, byte c, byte d);

// send event
void midi_output_event(byte a, byte b, byte c, byte d);

// enum input
void midi_enum_input(midi_enum_callback &callbcak);

// enum input
void midi_enum_output(midi_enum_callback &callbcak);

// rest midi
void midi_reset();

// update midi
void midi_update(double time);

// get key status
byte midi_get_note_status(byte ch, byte note);