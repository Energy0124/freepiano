#pragma once
#include <cmath>

template<class T>
inline T clamp_value(T value, T min, T max) {
  if (value < min) value = min;
  if (value > max) value = max;
  return value;
}

template<class T>
inline T wrap_value(T value, T min, T max) {
  if (value < min) value = max;
  if (value > max) value = min;
  return value;
}

inline float round(float x) {
  return floor(x + 0.5f);
}

template<class T>
inline bool point_in_rect(T x, T y, T rect_x, T rect_y, T rect_w, T rect_h) {
  return x >= rect_x && x < rect_x + rect_w &&
         y >= rect_y && y < rect_y + rect_h;
}