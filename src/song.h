#pragma once

// start record
void song_start_record();

// stop record
void song_stop_record();

// is recoding
bool song_is_recording();

// record event
void song_record_event(byte a, byte b, byte c, byte d);

// start playback
void song_start_playback();

// stop playback
void song_stop_playback();

// is recoding
bool song_is_playing();

// allow save
bool song_allow_save();

// song is empty
bool song_is_empty();

// update
void song_update(double time_elapsed);

// get record length
int song_get_length();


// open lyt
int song_open_lyt(const char * filename);

// close
void song_close();

// save song
int song_save(const char * filename);

// open song
int song_open(const char * filename);

// get time
int song_get_time();