#pragma once


#define	INSTRUMENT_TYPE_NONE		0
#define	INSTRUMENT_TYPE_VSTI		1

#define	OUTPUT_TYPE_NONE			0
#define	OUTPUT_TYPE_DSOUND			1
#define	OUTPUT_TYPE_WASAPI			2
#define	OUTPUT_TYPE_ASIO			3

// initialize config
int config_init();

// close config
void config_shutdown();

// load
int config_load(const char * filename);

// save
int config_save(const char * filename);

// select midi input
int config_select_midi_input(const char * device);

// get midi input
const char * config_get_midi_input();

// select midi output
int config_select_midi_output(const char * device);

// get midi output
const char * config_get_midi_output();


// select output
int config_select_output(int type, const char * device);

// get output type
int config_get_output_type();

// get output device
const char * config_get_output_device();

// get output delay
int config_get_output_delay();

// set output delay
void config_set_output_delay(int value);


// select instrument
int config_select_instrument(int type, const char * name);

// get instrument type
int config_get_instrument_type();

// get instrument
const char * config_get_instrument_path();


// get keymap
const char * config_get_keymap();

// set keymap
int config_set_keymap(const char * mapname);

// save keymap
int config_save_keymap(int key, char * buff, int buffer_size);

// keymap config
int config_keymap_config(char * command);

// get media path
void config_get_media_path(char * buff, int buff_size, const char * path);

// get volume
int config_get_output_volume();

// set volume
void config_set_output_volume(int volume);