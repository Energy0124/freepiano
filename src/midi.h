#pragma once

struct midi_enum_callback
{
	virtual void operator ()(const char * value) = 0;
};

// open output device
int midi_open_output(const char * name);

// close output device
void midi_close_output();

// open input device
int midi_open_input(const char * name);

// close input device
void midi_close_input();

// send event
void midi_send_event(byte data1, byte data2, byte data3, byte data4);

// set key signature
void midi_set_key_signature(char key);

// get key signature
char midi_get_key_signature();

// modify event
void midi_modify_event(byte & data1, byte & data2, byte & data3, byte & data4, char shift, byte velocity);

// enum input
void midi_enum_input(midi_enum_callback & callbcak);

// enum input
void midi_enum_output(midi_enum_callback & callbcak);

// rest midi
void midi_reset();

// get controller value
char midi_get_controller_value(byte control);