#pragma once

#define SMS_KEY_EVENT           0
#define SMS_KEY_MAP             1
#define SMS_KEY_LABEL           2

// FreePiano 1.0 messages
#define SM_SYSTEM               0
#define SM_KEY_SIGNATURE        1
#define SM_OCTSHIFT             2
#define SM_VELOCITY             3
#define SM_CHANNEL              4
#define SM_VOLUME               5
#define SM_PLAY                 6
#define SM_RECORD               7
#define SM_STOP                 8

// FreePiano 1.1 messages
#define SM_SETTING_GROUP        9
#define SM_SETTING_GROUP_COUNT  10
#define SM_AUTO_PEDAL           11
#define SM_DELAY_KEYUP          12

// FreePiano 1.7 messages
#define SM_TRANSPOSE            13

struct song_info_t {
  uint version;
  char title[256];
  char author[256];
  char comment[1024];
  bool write_protected;
};

// send event message
void song_send_event(byte a, byte b, byte c, byte d, bool record = false);

// output event
void song_output_event(byte a, byte b, byte c, byte d);

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

// get clock
double song_get_clock();

// song get play speed
double song_get_play_speed();

// song get play speed
void song_set_play_speed(double speed);

// open lyt
int song_open_lyt(const char *filename);

// close
void song_close();

// save song
int song_save(const char *filename);

// open song
int song_open(const char *filename);

// get song info
song_info_t* song_get_info();

