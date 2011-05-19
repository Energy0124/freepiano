#pragma once

struct song_info_t
{
	uint version;
	char title[256];
	char author[256];
	char comment[1024];
	bool write_protected;
};

// send event message
void song_send_event(byte a, byte b, byte c, byte d, bool record = false);

// reset
void song_reset_event();


// start record
void song_start_record();

// stop record
void song_stop_record();

// is recoding
bool song_is_recording();

// start playback
void song_start_playback();

// stop playback
void song_stop_playback();

// is recoding
bool song_is_playing();

// allow save
bool song_allow_save();

// allow input
bool song_allow_input();

// song is empty
bool song_is_empty();

// update
void song_update(double time_elapsed);

// get record length
int song_get_length();

// get time
int song_get_time();

// open lyt
int song_open_lyt(const char * filename);

// close
void song_close();

// save song
int song_save(const char * filename);

// open song
int song_open(const char * filename);

// get song info
song_info_t * song_get_info();