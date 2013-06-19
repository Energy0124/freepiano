#pragma once

#define SMS_KEY_EVENT             0x00
#define SMS_KEY_NOP               0x00
#define SMS_KEY_MAP             	0x01
#define SMS_KEY_LABEL           	0x02

// FreePiano 1.0 messages
#define SM_SYSTEM                 0x00
#define SM_KEY_SIGNATURE        	0x01
#define SM_OCTAVE               	0x02
#define SM_VELOCITY             	0x03
#define SM_CHANNEL              	0x04
#define SM_VOLUME               	0x05
#define SM_PLAY                 	0x06
#define SM_RECORD               	0x07
#define SM_STOP                 	0x08

// FreePiano 1.1 messages
#define SM_SETTING_GROUP          0x09
#define SM_SETTING_GROUP_COUNT  	0x0a
#define SM_AUTO_PEDAL_OBSOLETE  	0x0b
#define SM_DELAY_KEYUP_OBSOLETE 	0x0c

// FreePiano 1.7 messages
#define SM_TRANSPOSE              0x0d

// for compatibility
#define SM_CONTROLLER_DEPRECATED  0x0e

// FreePiano 1.8 messages
#define SM_NOTE_OFF               0x10
#define SM_NOTE_ON                0x11
#define SM_NOTE_PRESSURE          0x12
#define SM_PRESSURE               0x13
#define SM_PITCH                  0x14
#define SM_PROGRAM                0x15
#define SM_BANK_MSB               0x16
#define SM_BANK_LSB               0x17
#define SM_SUSTAIN                0x18

// value op
#define SM_VALUE_SET              0x00
#define SM_VALUE_INC              0x01
#define SM_VALUE_DEC              0x02
#define SM_VALUE_FLIP             0x03
#define SM_VALUE_PRESS            0x04
#define SM_VALUE_SET10            0x0a
#define SM_VALUE_SET1             0x0b
#define SM_VALUE_SYNC             0x10

// MIDI messages
#define SM_MIDI_MASK_MSG          0xf0
#define SM_MIDI_MASK_CHANNEL      0x0f
#define SM_MIDI_MESSAGE_START     0x80
#define SM_MIDI_NOTEOFF           0x80
#define SM_MIDI_NOTEON            0x90
#define SM_MIDI_PRESSURE          0xa0
#define SM_MIDI_CONTROLLER        0xb0
#define SM_MIDI_PROGRAM           0xc0
#define SM_MIDI_CHANNEL_PRESSURE  0xd0
#define SM_MIDI_PITCH_BEND        0xe0
#define SM_MIDI_SYSEX             0xf0

struct song_info_t {
  uint version;
  char title[256];
  char author[256];
  char comment[1024];
  bool write_protected;
  bool compatibility;
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

// translate message
bool song_translate_event(byte &a, byte &b, byte &c, byte &d);

// trigger sync
void song_trigger_sync();

