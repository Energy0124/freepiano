#pragma once

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

// set oct shift
void midi_set_octshift(char shift);

// get oct shift
char midi_get_octshift();

// modify event
void midi_modify_event(byte & data1, byte & data2, byte & data3, byte & data4, char shift, byte velocity);