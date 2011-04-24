#pragma once

// open dsound.
int wasapi_open(const char * name);

// close dsound device
void wasapi_close();

// set buffer size
void wasapi_set_buffer_time(double time);
