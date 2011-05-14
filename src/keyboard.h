#pragma once
#include "midi.h"

struct KeyboardEvent
{
	byte action;
	byte arg1;
	byte arg2;
	byte arg3;

	KeyboardEvent()
		: action(0)
		, arg1(0)
		, arg2(0)
		, arg3(0)
	{}

	KeyboardEvent(byte a, byte b, byte c, byte d)
		: action(a)
		, arg1(b)
		, arg2(c)
		, arg3(d)
	{}
};

// keymap enum callback
struct keymap_enum_callback
{
	virtual void operator ()(const char * value) = 0;
}; 

// initialize keyboard
int keyboard_init();

// shutdown keyboard system
void keyboard_shutdown();

// enable or disable keyboard
void keyboard_enable(bool enable);

// get keyboard map
void keyboard_get_map(byte code, KeyboardEvent * keydown, KeyboardEvent * keyup);

// set keyboard action
void keyboard_set_map(byte code, KeyboardEvent * keydown, KeyboardEvent * keyup);

// get default keyup
bool keyboard_default_keyup(KeyboardEvent * keydown, KeyboardEvent * keyup);

// set oct shift
void keyboard_set_octshift(byte channel, char shift);

// get oct shift
char keyboard_get_octshift(byte channel);

// set velocity
void keyboard_set_velocity(byte channel, byte velocity);

// get velocity
byte keyboard_get_velocity(byte channel);

// get channel
int keyboard_get_channel(byte channel);

// get channel
void keyboard_set_channel(byte channel, byte value);

// event message
void keyboard_send_event(byte a, byte b, byte c, byte d);

// enum keymap
void keyboard_enum_keymap(keymap_enum_callback & callback);

// reset keyboard
void keyboard_reset();