#pragma once

struct asio_enum_callback {
  virtual void operator () (const char *value) = 0;
};

// open driver
int asio_open(const char *driver_name);

// close driver
void asio_close();

// enum device
void asio_enum_device(asio_enum_callback &callback);