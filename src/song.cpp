#include "pch.h"

#include <dinput.h>
#include <Shlwapi.h>

#include <zlib.h>

#include "song.h"
#include "display.h"
#include "midi.h"
#include "keyboard.h"
#include "config.h"
#include "gui.h"
#include "language.h"
#include "utilities.h"

struct song_event_t {
  double time;
  byte a;
  byte b;
  byte c;
  byte d;
};

// record buffer (1M events should be enough.)
static song_event_t song_event_buffer[1024 * 1024];

// record or play position
static song_event_t *play_position = NULL;
static song_event_t *record_position = NULL;
static song_event_t *song_end = NULL;
static double song_timer = 0;
static double song_play_speed = 1;
static double song_auto_pedal_timer = 0;
static double song_clock = 0;

static song_info_t song_info;

// song thread lock
static thread_lock_t song_lock;

// current version
static uint current_version = 0x01080000;



// -----------------------------------------------------------------------------------------
// event translator
// -----------------------------------------------------------------------------------------

// controller save state
static struct controller_state_t {
  double restore_timer;
  byte   restore_value;
  bool sync_wait;
  byte sync_value;
}
midi_controller_state[16][128] = {0};

// sync trigger
static bool midi_sync_trigger = false;

static inline byte translate_channel(byte ch) {
  return config_get_key_channel(ch);
}

static inline byte translate_note(byte ch, byte note) {
  return clamp_value((int)note + config_get_key_octshift(ch) * 12 + config_get_key_transpose(ch) + config_get_key_signature(), 0, 127);
}

static inline byte translate_pressure(byte ch, byte pressure) {
  return clamp_value((int)pressure * config_get_key_velocity(ch) / 127, 0, 127);
}

static inline int default_value(int v, int dv = 0) {
  if (v > 127) return dv;
  if (v < -127) return dv;
  return v;
}

static int translate_value(byte action, int value, int change) {
  switch (action & 0x0f) {
  case SM_VALUE_SET: value = change; break;
  case SM_VALUE_INC: value = value + change; break;
  case SM_VALUE_DEC: value = value - change; break;
  case SM_VALUE_FLIP: value = change - value; break;
  case SM_VALUE_PRESS: value = change; break;
  case SM_VALUE_SET10: value = change / 10 * 10 + (value % 10); break;
  case SM_VALUE_SET1: value = value / 10 * 10 + (change % 10); break;
  }
  return value;
}

static bool translate_controller(byte &a, byte &b, byte &c, byte &d, int id) {
  byte ch = config_get_key_channel(b);
  byte op = c;
  int change = (char)d;
  int value = config_get_controller(ch, id);
  

  // sync or press
  if ((op & 0x0f) == SM_VALUE_PRESS || (op & 0x10)) {
    return false;
  }

  value = default_value(value);
  value = translate_value(op, value, change);
  value = clamp_value(value, 0, 127);

  a = SM_MIDI_CONTROLLER | ch;
  b = id;
  c = value;
  d = 0;
  return true;
}

// translate message
bool song_translate_event(byte &a, byte &b, byte &c, byte &d) {
  switch (a) {
  case SM_NOTE_ON:
    {
      byte ch = b;
      byte note = c;
      byte pressure = d;
      a = SM_MIDI_NOTEON | translate_channel(ch);
      b = translate_note(ch, note);
      c = translate_pressure(ch, pressure);
      d = 0;
      return true;
    }
    break;
  case SM_NOTE_OFF:
    {
      a = SM_MIDI_NOTEOFF | config_get_key_channel(b);
      b = translate_note(b, c);
      c = 0;
      d = 0;
      return true;
    }
    break;

  case SM_NOTE_PRESSURE:
    {
      byte ch = b;
      byte note = c;
      byte pressure = d;
      a = SM_MIDI_PRESSURE | translate_channel(ch);
      b = translate_note(ch, note);
      c = translate_pressure(ch, pressure);
      d = 0;
      return true;
    }
    break;

  case SM_PRESSURE:
    {
      byte ch = b;
      byte pressure = c;
      a = SM_MIDI_CHANNEL_PRESSURE | translate_channel(ch);
      b = translate_pressure(ch, pressure);
      c = 0;
      d = 0;
      return true;
    }
    break;

  case SM_PITCH:
    {
      byte ch = b;
      a = SM_MIDI_PITCH_BEND | translate_channel(ch);
      b = c;
      c = d;
      d = 0;
      return true;
    }
    break;

  case SM_PROGRAM:
    {
      byte ch = config_get_key_channel(b);
      byte op = c;
      char change = d;
      int value = config_get_program(ch);
      value = default_value(value);
      value = translate_value(op, value, change);

      a = SM_MIDI_PROGRAM | ch;
      b = clamp_value(value, 0, 127);
      c = 0;
      d = 0;

      return true;
    }
    break;

  case SM_BANK_LSB:
    return translate_controller(a, b, c, d, 0x20);

  case SM_BANK_MSB:
    return translate_controller(a, b, c, d, 0x00);

  case SM_SUSTAIN:
    return translate_controller(a, b, c, d, 64);
  }

  return false;
}

// -----------------------------------------------------------------------------------------
// event parser
// -----------------------------------------------------------------------------------------

// add event
static void song_add_event(double time, byte a, byte b, byte c, byte d);

// dynamic mapping
static byte keyboard_map_key_code = 0;
static byte keyboard_map_key_type = 0;
static byte keyboard_label_key_code = 0;
static byte keyboard_label_key_size = 0;
static char keyboard_label_text[256];

// keyboard event map
static void keyboard_event_map(int code, int type) {
  keyboard_map_key_code = code;
  keyboard_map_key_type = type;
}

// keyboard event label
static void keyboard_event_label(int code, int length) {
  keyboard_label_key_code = code;
  keyboard_label_key_size = length;
  keyboard_label_text[0] = 0;
}

// event message
void song_send_event(byte a, byte b, byte c, byte d, bool record) {
  thread_lock lock(song_lock);

  // record event
  if (record) {
    // HACK: don't record playback control commands
    if (a != SM_PLAY &&
        a != SM_RECORD &&
        a != SM_STOP)
      song_add_event(song_timer, a, b, c, d);
  }

  // setting a key label
  if (keyboard_label_key_size) {
    char *ch = keyboard_label_text + strlen(keyboard_label_text);
    char data[4] = { a, b, c, d };

    for (int i = 0; i < 4; i++) {
      ch[i] = data[i];
      ch[i + 1] = 0;

      if (--keyboard_label_key_size == 0) {
        config_bind_set_label(keyboard_label_key_code, keyboard_label_text);
        break;
      }
    }
    return;
  }

  // mapping a key.
  if (keyboard_map_key_code) {
    switch (keyboard_map_key_type) {
     case 0: config_bind_add_keydown(keyboard_map_key_code, key_bind_t(a, b, c, d)); break;
     case 1: config_bind_add_keyup(keyboard_map_key_code, key_bind_t(a, b, c, d)); break;
    }

    keyboard_map_key_code = 0;
    return;
  }

  // special events
  if (a == SM_SYSTEM) {
    switch (b) {
     case SMS_KEY_EVENT: keyboard_send_event(c, d); break;
     case SMS_KEY_MAP:   keyboard_event_map(c, d); break;
     case SMS_KEY_LABEL: keyboard_event_label(c, d); break;
    }
    return;
  }

  song_output_event(a, b, c, d);
}

static void output_controller(byte a, byte b, byte c, byte d, byte id) {
  byte ch = config_get_key_channel(b);
  byte op = c;
  int change = (char)d;
  int value = 0;

  // midi controller state
  controller_state_t &state = midi_controller_state[ch][id];

  value = config_get_controller(ch, id);
  value = default_value(value);
  value = translate_value(op, value, change);
  value = clamp_value(value, 0, 127);

  // press
  if ((op & 0x0f) == SM_VALUE_PRESS) {
    state.restore_timer = 20;
    state.restore_value = config_get_controller(ch, id);
  }

  // sync
  if (op & 0x10) {
    state.sync_wait = true;
    state.sync_value = value;
    return;
  }

  midi_output_event(SM_MIDI_CONTROLLER | ch, id, value, 0);
}

void song_output_event(byte a, byte b, byte c, byte d) {
  // midi events
  if (a >= SM_MIDI_MESSAGE_START) {
    midi_output_event(a, b, c, d);
    return;
  }

  // keyboard control events
  switch (a) {
   case SM_KEY_SIGNATURE: {
     int value = config_get_key_signature();

     value = translate_value(b, value, (char)c);
     value = wrap_value(value, -4, 7);

     config_set_key_signature(value);
   }
   break;

   case SM_TRANSPOSE: {
     int ch = b;
     int value = config_get_key_transpose(ch);

     value = translate_value(c, value, (char)d);
     value = clamp_value(value, -64, 64);

     config_set_key_transpose(ch, value);
   }
   break;

   case SM_OCTAVE: {
     int ch = b;
     int value = config_get_key_octshift(ch);

     value = translate_value(c, value, (char)d);
     value = wrap_value(value, -1, 1);

     config_set_key_octshift(ch, value);
   }
   break;

   case SM_VELOCITY: {
     int ch = b;
     int value = config_get_key_velocity(ch);

     value = translate_value(c, value, (char)d);
     value = clamp_value(value, 0, 127);

     config_set_key_velocity(ch, value);
   }
   break;

   case SM_CHANNEL: {
     int ch = b;
     int value = config_get_key_channel(ch);

     value = translate_value(c, value, (char)d);
     value = clamp_value(value, 0, 15);

     config_set_key_channel(ch, value);
   }
   break;

   case SM_VOLUME: {
     int value = config_get_output_volume();

     value = translate_value(b, value, c);
     value = clamp_value(value, 0, 200);

     config_set_output_volume(value);
   }
   break;

   case SM_PLAY:
     if (song_allow_input())
       song_start_playback();
     break;

   case SM_RECORD:
     if (song_allow_input()) {
       if (song_is_recording())
         song_stop_record();
       else
         song_start_record();
     }
     break;

   case SM_STOP:
     if (song_is_recording())
       song_stop_record();

     if (song_is_playing())
       song_stop_playback();
     break;

   case SM_SETTING_GROUP: {
     int value = config_get_setting_group();

     value = translate_value(b, value, (char)c);
     value = wrap_value(value, 0, (int)config_get_setting_group_count() - 1);

     config_set_setting_group(value);
   }
   break;

   case SM_SETTING_GROUP_COUNT: {
     config_set_setting_group_count(b);
   }
   break;

   case SM_NOTE_ON:
   case SM_NOTE_OFF:
   case SM_NOTE_PRESSURE:
   case SM_PRESSURE:
   case SM_PITCH:
   case SM_PROGRAM:
     {
       if (song_translate_event(a, b, c, d)) {
         midi_output_event(a, b, c, d);
       }
     }
     break;

  case SM_BANK_LSB:
    output_controller(a, b, c, d, 32);
    break;

  case SM_BANK_MSB:
    output_controller(a, b, c, d, 0);
    break;

  case SM_SUSTAIN:
    output_controller(a, b, c, d, 64);
    break;
  }
}

// -----------------------------------------------------------------------------------------
// record and playback
// -----------------------------------------------------------------------------------------

// add event
static void song_add_event(double time, byte a, byte b, byte c, byte d) {
  if (record_position) {
    record_position->time = time;
    record_position->a = a;
    record_position->b = b;
    record_position->c = c;
    record_position->d = d;

    ++record_position;

    // just simple set song end to record posision
    song_end = record_position;

    // auto stop record
    if (record_position == ARRAY_END(song_event_buffer) - 1) {
      song_stop_record();
    }
  }
}

// init record
static void song_init_record() {
  song_timer = 0;
  song_clock = 0;
  song_auto_pedal_timer = 0;
  record_position = song_event_buffer;
  play_position = NULL;
  song_end = song_event_buffer;
  song_info.version = current_version;
  song_info.author[0] = 0;
  song_info.title[0] = 0;
  song_info.comment[0] = 0;
  song_info.write_protected = false;

  song_play_speed = 1;
}

// reset
static void song_reset_event() {
  // exit key map mode
  keyboard_map_key_code = 0;

  // exit key label mode
  keyboard_label_key_size = 0;

  // reset keyboard
  keyboard_reset();

  // clear controller save
  memset(midi_controller_state, 0, sizeof(midi_controller_state));

  // reset midi
  midi_reset();

  // reset sync trigger
  midi_sync_trigger = false;
}

// start record
void song_start_record() {
  thread_lock lock(song_lock);

  song_stop_playback();
  song_stop_record();
  song_init_record();

  int current_group = config_get_setting_group();
  int total_groups = config_get_setting_group_count();

  // record config groups
  song_add_event(0, SM_SETTING_GROUP_COUNT, total_groups, 0, 0);

  // record group settings
  for (int i = 0; i < total_groups; i++) {
    // change current setting group
    config_set_setting_group(i);
    song_add_event(0, SM_SETTING_GROUP, 0, i, 0);

    // record current configs
    song_add_event(0, SM_KEY_SIGNATURE, 0, config_get_key_signature(), 0);

    // record key settings
    for (int ch = 0; ch < 16; ch++) {
      if (config_get_key_octshift(ch))
        song_add_event(0, SM_OCTAVE, ch, 0, config_get_key_octshift(ch));

      if (config_get_key_transpose(ch))
        song_add_event(0, SM_TRANSPOSE, ch, 0, config_get_key_transpose(ch));

      if (config_get_key_velocity(ch) != 127)
        song_add_event(0, SM_VELOCITY, ch, 0, config_get_key_velocity(ch));

      if (config_get_key_channel(ch))
        song_add_event(0, SM_CHANNEL, ch, 0, config_get_key_channel(ch));

      if (config_get_program(ch) < 128)
        song_add_event(0, 0xc0 | ch, config_get_program(ch), 0, 0);

      for (int id = 0; id < 128; id++) {
        if (config_get_controller(ch, id) < 128)
          song_add_event(0, 0xb0 | ch, id, config_get_controller(ch, id), 0);
      }
    }

    // record keymaps
    for (int i = 0; i < 256; i++) {
      int count;
      key_bind_t bind[256];

      count = config_bind_get_keydown(i, bind, 256);
      for (int j = 0; j < count; j++) {
        if (bind[j].a) {
          song_add_event(0, SM_SYSTEM, SMS_KEY_MAP, i, 0);
          song_add_event(0, bind[j].a, bind[j].b, bind[j].c, bind[j].d);
        }
      }

      count = config_bind_get_keyup(i, bind, 256);
      for (int j = 0; j < count; j++) {
        if (bind[j].a) {
          song_add_event(0, SM_SYSTEM, SMS_KEY_MAP, i, 1);
          song_add_event(0, bind[j].a, bind[j].b, bind[j].c, bind[j].d);
        }
      }

      if (*config_bind_get_label(i)) {
        const char *label = config_bind_get_label(i);
        int size = strlen(label);

        song_add_event(0, SM_SYSTEM, SMS_KEY_LABEL, i, size);

        for (int i = 0; i < (size + 3) / 4; i++) {
          song_add_event(0, label[0], label[1], label[2], label[3]);
          label += 4;
        }
      }
    }
  }

  // restore current group
  config_set_setting_group(current_group);
  song_add_event(0, SM_SETTING_GROUP, 0, current_group, 0);

  // pedal
  song_add_event(0, 0xb0, 0x40, config_get_controller(0, 0x40), 0);
}

// stop record
void song_stop_record() {
  thread_lock lock(song_lock);

  if (record_position) {
    song_add_event(song_timer, SM_STOP, 0, 0, 0);
    song_reset_event();
    record_position = NULL;
  }
}

// is recoding
bool song_is_recording() {
  thread_lock lock(song_lock);

  return record_position != NULL;
}

// start playback
void song_start_playback() {
  thread_lock lock(song_lock);

  if (song_end) {
    song_stop_record();
    song_stop_playback();

    song_timer = 0;
    song_clock = 0;
    song_auto_pedal_timer = 0;
    play_position = song_event_buffer;

    // clear current setting
    config_set_setting_group_count(1);
    config_clear_key_setting();

    song_update(0);
  }
}

// stop playback
void song_stop_playback() {
  thread_lock lock(song_lock);

  song_reset_event();
  play_position = NULL;
}

// is recoding
bool song_is_playing() {
  thread_lock lock(song_lock);

  return play_position != NULL;
}


// get record length
int song_get_length() {
  thread_lock lock(song_lock);

  if (song_end && song_end > song_event_buffer)
    return (int)song_end[-1].time;

  return 0;
}

// allow save
bool song_allow_save() {
  thread_lock lock(song_lock);
  return song_end != NULL && !song_info.write_protected && !song_is_recording();
}

// allow input
bool song_allow_input() {
  thread_lock lock(song_lock);
  return play_position == NULL;
}

// song has data
bool song_is_empty() {
  thread_lock lock(song_lock);
  return song_end == NULL;
}


// get time
int song_get_time() {
  thread_lock lock(song_lock);
  return (int)song_timer;
}

// get clock
double song_get_clock() {
  thread_lock lock(song_lock);
  return song_clock;
}

// song get play speed
double song_get_play_speed() {
  return song_play_speed;
}

// song get play speed
void song_set_play_speed(double speed) {
  song_play_speed = speed;
  if (song_play_speed < 0)
    song_play_speed = 0;
}

// update
void song_update(double time_elapsed) {
  thread_lock lock(song_lock);

#ifdef _DEBUG
  if (GetAsyncKeyState(VK_ESCAPE))
    return;
#endif

  // adjust playing speed
  if (song_is_playing())
    time_elapsed *= song_play_speed;

  if (song_is_playing() || song_is_recording())
    song_timer += time_elapsed;

  // playback
  while (play_position && song_end) {
    if (play_position->time <= song_timer) {
      // send event to keyboard
      song_send_event(play_position->a, play_position->b, play_position->c, play_position->d);

      if (play_position) {
        if (++play_position >= song_end) {
          song_stop_playback();
        }
      }
    } else break;
  }

  // adjust clock
  song_clock += time_elapsed;

  // update keyboard
  keyboard_update(time_elapsed);

  // update controllers
  for (int ch = 0; ch < 16; ch++) {
    for (int id = 0; id < 128; id++) {
      // midi controller state
      controller_state_t &state = midi_controller_state[ch][id];

      // sync controller
      if (state.sync_wait) {
        if (midi_sync_trigger) {
          state.sync_wait = false;
          midi_output_event(0xb0 | ch, id, state.sync_value, 0);
        }
      }

      // restore mode
      else if (state.restore_timer > 0) {
        state.restore_timer -= time_elapsed;

        if (state.restore_timer <= 0) {
          midi_output_event(0xb0 | ch, id, state.restore_value, 0);
        }
      }
    }
  }

  midi_sync_trigger = false;
}

song_info_t* song_get_info() {
  return &song_info;
}

void song_trigger_sync() {
  midi_sync_trigger = true;
}

// -----------------------------------------------------------------------------------------
// load and save functions
// -----------------------------------------------------------------------------------------

static void read(void *buff, int size, FILE *fp) {
  int ret = fread(buff, 1, size, fp);
  if (ret != size)
    throw -1;
}

static void write(const void *buff, int size, FILE *fp) {
  int ret = fwrite(buff, 1, size, fp);
  if (ret != size)
    throw -1;
}

static void read_string(char *buff, size_t buff_size, FILE *fp) {
  uint count = 0;

  read(&count, sizeof(count), fp);

  if (count < buff_size) {
    read(buff, count, fp);
    buff[count] = 0;
  } else   {
    throw -1;
  }
}

static void write_string(const char *buff, FILE *fp) {
  uint count = strlen(buff);

  write(&count, sizeof(count), fp);
  write(buff, count, fp);
}

static void read_idp_string(char *buff, size_t buff_size, FILE *fp) {
  byte count = 0;
  read(&count, sizeof(count), fp);

  if (count < buff_size) {
    read(buff, buff_size, fp);
    buff[count] = 0;
  } else   {
    throw -1;
  }
}

// open lyt
int song_open_lyt(const char *filename) {
  thread_lock lock(song_lock);

  song_close();

  FILE *fp = fopen(filename, "rb");
  try {
    char header[16];

    if (!fp)
      return -1;

    // read magic
    read(header, 16, fp);
    if (strcmp(header, "iDreamPianoSong") != 0)
      throw -1;

    // start record
    song_init_record();

    // read header
    read(&song_info.version, sizeof(song_info.version), fp);
    read_idp_string(song_info.title, 39, fp);
    read_idp_string(song_info.author, 19, fp);
    read_idp_string(song_info.comment, 255, fp);

    // keymapping
    struct keymap_t {
      int midi_shift;
      int velocity_right;
      int velocity_left;
      int shift_right;
      int shift_left;
      int unknown1;
      int unknown2;
      int voice1;
      int unknown3;
      int voice2;
      int unknown4;
    };


    keymap_t map;
    read(&map, sizeof(map), fp);

    // record set params command
    song_add_event(0, SM_KEY_SIGNATURE, SM_VALUE_SET, map.midi_shift, 0);
    song_add_event(0, SM_OCTAVE, 0, SM_VALUE_SET, map.shift_left);
    song_add_event(0, SM_OCTAVE, 1, SM_VALUE_SET, map.shift_right);
    song_add_event(0, SM_VELOCITY, 0, SM_VALUE_SET, map.velocity_left);
    song_add_event(0, SM_VELOCITY, 1, SM_VALUE_SET, map.velocity_right);

    // push down padel
    if (map.voice1 > 0 || map.voice2 > 0)
      song_add_event(0, 0xb0, 0x40, 127, 0);

    if (song_info.version > 1) {
      static const byte keycode[] = {
        DIK_ESCAPE,
        DIK_F1,
        DIK_F2,
        DIK_F3,
        DIK_F4,
        DIK_F5,
        DIK_F6,
        DIK_F7,
        DIK_F8,
        DIK_F9,
        DIK_F10,
        DIK_F11,
        DIK_F12,
        DIK_GRAVE,
        DIK_1,
        DIK_2,
        DIK_3,
        DIK_4,
        DIK_5,
        DIK_6,
        DIK_7,
        DIK_8,
        DIK_9,
        DIK_0,
        DIK_MINUS,
        DIK_EQUALS,
        DIK_BACKSLASH,
        DIK_BACKSPACE,
        DIK_TAB,
        DIK_Q,
        DIK_W,
        DIK_E,
        DIK_R,
        DIK_T,
        DIK_Y,
        DIK_U,
        DIK_I,
        DIK_O,
        DIK_P,
        DIK_LBRACKET,
        DIK_RBRACKET,
        DIK_RETURN,
        DIK_CAPITAL,
        DIK_A,
        DIK_S,
        DIK_D,
        DIK_F,
        DIK_G,
        DIK_H,
        DIK_J,
        DIK_K,
        DIK_L,
        DIK_SEMICOLON,
        DIK_APOSTROPHE,
        DIK_LSHIFT,
        DIK_Z,
        DIK_X,
        DIK_C,
        DIK_V,
        DIK_B,
        DIK_N,
        DIK_M,
        DIK_COMMA,
        DIK_PERIOD,
        DIK_SLASH,
        DIK_RSHIFT,
        DIK_LCONTROL,
        DIK_LWIN,
        DIK_LMENU,
        DIK_SPACE,
        DIK_RMENU,
        DIK_RWIN,
        DIK_APPS,
        DIK_RCONTROL,
        DIK_WAKE,
        DIK_SLEEP,
        DIK_POWER,
        DIK_SYSRQ,
        DIK_SCROLL,
        DIK_PAUSE,
        DIK_INSERT,
        DIK_HOME,
        DIK_PGUP,
        DIK_DELETE,
        DIK_END,
        DIK_PGDN,
        DIK_UP,
        DIK_LEFT,
        DIK_DOWN,
        DIK_RIGHT,
        DIK_NUMLOCK,
        DIK_NUMPADSLASH,
        DIK_NUMPADSTAR,
        DIK_NUMPADMINUS,
        DIK_NUMPAD7,
        DIK_NUMPAD8,
        DIK_NUMPAD9,
        DIK_NUMPADPLUS,
        DIK_NUMPAD4,
        DIK_NUMPAD5,
        DIK_NUMPAD6,
        DIK_NUMPAD1,
        DIK_NUMPAD2,
        DIK_NUMPAD3,
        DIK_NUMPADENTER,
        DIK_NUMPAD0,
        DIK_NUMPADPERIOD,
      };

      static const int notes[] = { 0, 0, 2, 4, 5, 7, 9, 11 };
      static const int shifts[] = { 0, 1, -1};

      // set keymap
      for (int i = 0; i < 107; i++) {
        struct key_t {
          int enabled;
          byte note;
          byte shift;
          byte octive;
          byte channel;

          char unknown[0x44];
        };

        key_t key;
        read(&key, sizeof(key), fp);

        if (key.enabled && key.note) {
          key_bind_t keydown, keyup;
          keydown.a = SM_NOTE_ON;
          keydown.b = (key.channel & 0xf);
          keydown.c = 12 + key.octive * 12 + notes[key.note % 8] + shifts[key.shift % 3];
          keydown.d = 127;

          // add keymap event
          song_add_event(0, SM_SYSTEM, SMS_KEY_MAP, keycode[i], 0);
          song_add_event(0, keydown.a, keydown.b, keydown.c, keydown.d);
        }
      }



      // keymap description
      char keymap_description[240];
      read_idp_string(keymap_description, 239, fp);
    }

    int unknown;
    int event_count;
    read(&unknown, sizeof(unknown), fp);
    read(&event_count, sizeof(event_count), fp);


    for (int i = 0; i < event_count; i++) {
      int time;
      int unknown;
      byte type;
      int key;

      read(&time, sizeof(time), fp);
      read(&unknown, sizeof(unknown), fp);

      read(&type, sizeof(type), fp);
      read(&key, sizeof(key), fp);

      switch (type) {
       case 1: song_add_event(time, SM_SYSTEM, SMS_KEY_EVENT, key, 1); break;      // keydown
       case 2: song_add_event(time, SM_SYSTEM, SMS_KEY_EVENT, key, 0); break;      // keyup
       case 3: song_add_event(time, SM_OCTAVE, 1, SM_VALUE_SET, key); break;       // set oct shift
       case 4: song_add_event(time, SM_OCTAVE, 0, SM_VALUE_SET, key); break;       // set oct shift
       case 5: song_add_event(time, SM_KEY_SIGNATURE, SM_VALUE_SET, key, 0); break;// set midi oct shift
       case 18:    break;      // drum
       case 19:    break;      // maybe song end.
       default:
         printf("unknown type : %d\t%d\t%d\n", time, type, key);
      }
    }

    record_position = NULL;
    fclose(fp);

    // mark song protected
    song_info.write_protected = true;
  } catch (int err) {
    song_end = NULL;
    fclose(fp);
    return err;
  }

  return 0;
}


// close
void song_close() {
  thread_lock lock(song_lock);

  song_stop_record();
  song_stop_playback();
  song_end = NULL;
}


// check compatibility
static void check_compatibility() {
  // upgrade from 1.7 to 1.8
  if (song_info.version <= 0x01070000) {
    song_event_t *e;

    // foreach event
    for (e = song_event_buffer; e < song_end; e++) {
      byte a = e->a;
      byte b = e->b;
      byte c = e->c;
      byte d = e->d;

      if (a == SM_SYSTEM) {
        if (b == SMS_KEY_LABEL) {
          e += (d + 3) / 4;
        }
      }

      else if (a == SM_AUTO_PEDAL_OBSOLETE) {
        if (b != 0 || c != 0)
          song_info.compatibility = false;
      }

      else if (a == SM_DELAY_KEYUP_OBSOLETE) {
        if (c != 0 || d != 0)
          song_info.compatibility = false;
      }
      else
      {
        switch (a & 0xf0) {
        case SM_MIDI_NOTEON:
          e->a = SM_NOTE_ON;
          e->b = a & 0x0f;
          e->c = b;
          e->d = c;
          break;

        case SM_MIDI_NOTEOFF:
          e->a = SM_NOTE_OFF;
          e->b = a & 0x0f;
          e->c = b;
          e->d = 0;
          break;

        case SM_MIDI_PROGRAM:
          e->a = SM_PROGRAM;
          e->b = a & 0x0f;
          e->c = c;
          e->d = b;
          break;

        case SM_MIDI_CONTROLLER:
          if (b == 64) {
            e->a = SM_SUSTAIN;
            e->b = a & 0x0f;
            e->c = d;
            e->d = c;

            // flip needs a value since 1.8
            if (d == SM_VALUE_FLIP) {
              e->d = 127;
            }
          }
          break;
        }
      }
    }
  }

}

// open song
int song_open(const char *filename) {
  thread_lock lock(song_lock);

  song_close();

  FILE *fp = fopen(filename, "rb");

  try {
    if (!fp)
      throw -1;

    char magic[sizeof("FreePianoSong")];
    char instrument[256];

    // magic
    read(magic, sizeof("FreePianoSong"), fp);
    if (memcmp(magic, "FreePianoSong", sizeof("FreePianoSong")) != 0)
      throw -1;

    // start record
    song_init_record();

    // version
    read(&song_info.version, sizeof(song_info.version), fp);
    if (song_info.version > current_version)
      throw -2;

    // song info
    read_string(song_info.title, sizeof(song_info.title), fp);
    read_string(song_info.author, sizeof(song_info.author), fp);
    read_string(song_info.comment, sizeof(song_info.comment), fp);

    // instrument
    read_string(instrument, sizeof(instrument), fp);


    // read events
    uint temp_size;
    read(&temp_size, sizeof(temp_size), fp);
    byte *temp_buffer = new byte[temp_size];

    read(temp_buffer, temp_size, fp);

    uint data_size = sizeof(song_event_buffer);
    uncompress((byte *)song_event_buffer, (uLongf *)&data_size, (Bytef *)temp_buffer, temp_size);

    song_end = record_position = song_event_buffer + data_size / sizeof(song_event_t);

    record_position = NULL;

    delete[] temp_buffer;
    fclose(fp);

    // mark song protected
    song_info.write_protected = true;

    // check compatibility and perhaps upgrade to lastest version.
    check_compatibility();

    return 0;
  } catch (int err) {
    song_end = NULL;
    if (fp) fclose(fp);
    return err;
  }
}


// save song
int song_save(const char *filename) {
  thread_lock lock(song_lock);

  song_stop_record();

  if (song_end) {
    FILE *fp = fopen(filename, "wb");
    if (!fp)
      return -1;
    try {
      // magic
      write("FreePianoSong", sizeof("FreePianoSong"), fp);

      // version
      song_info.version = current_version;
      write(&song_info.version, sizeof(song_info.version), fp);

      // song info
      write_string(song_info.title, fp);
      write_string(song_info.author, fp);
      write_string(song_info.comment, fp);

      // instrument
      {
        char buff[256];
        strncpy(buff, PathFindFileName(config_get_instrument_path()), sizeof(buff));
        PathRenameExtension(buff, "");
        write_string(buff, fp);
      }

      // write events
      uint temp_size = sizeof(song_event_buffer);
      byte *temp_buffer = new byte[temp_size];

      compress((Bytef *)temp_buffer, (uLongf *)&temp_size, (byte *)song_event_buffer, (song_end - song_event_buffer) * sizeof(song_event_t));

      write(&temp_size, sizeof(temp_size), fp);
      write(temp_buffer, temp_size, fp);

      delete[] temp_buffer;

      fclose(fp);
      return 0;
    } catch (int err) {
      fclose(fp);
      return err;
    }
  }
  return -1;
}