#pragma once

struct wasapi_enum_callback {
  virtual void operator () (const char *name, void *device) = 0;
};

// open dsound.
int wasapi_open(const char *name);

// close dsound device
void wasapi_close();

// set buffer size
void wasapi_set_buffer_time(double time);

// enum device
void wasapi_enum_device(wasapi_enum_callback &callback);

// get output samplerate
uint wasapi_get_samplerate();