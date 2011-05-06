#pragma once
#include "midi.h"

struct MidiEvent
{
	byte action;
	byte arg1;
	byte arg2;
	byte arg3;
};

// initialize keyboard
int keyboard_init();

// shutdown keyboard system
void keyboard_shutdown();

// enable or disable keyboard
void keyboard_enable(bool enable);

// get keyboard map
void keyboard_get_map(byte code, MidiEvent * keydown, MidiEvent * keyup);

// set keyboard action
void keyboard_set_map(byte code, MidiEvent * keydown, MidiEvent * keyup);

// get default keyup
bool keyboard_default_keyup(MidiEvent * keydown, MidiEvent * keyup);

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
void keyboard_send_event(uint code, uint keydown);