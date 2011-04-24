#pragma once;

// init display
int display_init();

// terminate display
int display_shutdown();

// get main window handle
HWND display_get_hwnd();

// send keyboard event to display system
void display_keyboard_event(byte code, uint status);

// send midi event to display system
void display_midi_event(byte data1, byte data2, byte data3, byte data4);

// show main window
void display_show();