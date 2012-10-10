#pragma once

struct dsound_enum_callback {
  virtual void operator () (const char *value) = 0;
};

// open dsound.
int dsound_open(const char *name);

// close dsound device
void dsound_close();

// set buffer size
void dsound_set_buffer_time(double time);

// enum device
void dsound_enum_device(dsound_enum_callback &callback);
