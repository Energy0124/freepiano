#include "pch.h"
#include "midi.h"
#include "synthesizer_vst.h"
#include "display.h"
#include "song.h"
#include "config.h"

#include <map>

static HMIDIOUT midi_out_device = NULL;
static HMIDIIN midi_in_device = NULL;

// midi thread lock
static thread_lock_t midi_input_lock;
static thread_lock_t midi_output_lock;

// note state
static byte note_states[16][128] = {0};

// auto generated keyup events
struct midi_keyup_t {
  byte midi_display_key;
  key_bind_t map;
};
static std::multimap<byte, midi_keyup_t> key_up_map;

// open output device
int midi_open_output(const char *name) {
  thread_lock lock(midi_output_lock);

  uint device_id = -1;
  midi_close_output();

  if (name && name[0]) {
    for (uint i = 0; i < midiOutGetNumDevs(); i++) {
      MIDIOUTCAPS caps;

      // get device caps
      if (midiOutGetDevCaps(i, &caps, sizeof(caps)))
        continue;

      if (_stricmp(name, caps.szPname) == 0) {
        device_id = i;
        break;
      }
    }
  }

  if (midiOutOpen(&midi_out_device, device_id, 0, 0, CALLBACK_NULL))
    return -1;

  return 0;
}

// close output device
void midi_close_output() {
  thread_lock lock(midi_output_lock);

  if (midi_out_device) {
    midiOutClose(midi_out_device);
    midi_out_device = NULL;
  }
}

static void CALLBACK midi_input_callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
  if (wMsg == MIM_DATA) {
    uint data = (uint)dwParam1;
    byte a = data >> 0;
    byte b = data >> 8;
    byte c = data >> 16;
    byte d = data >> 24;

    song_send_event(a & 0xf0, b, c, d, true);
  }
}

// open input device
int midi_open_input(const char *name) {
  thread_lock lock(midi_input_lock);

  uint device_id = -1;
  midi_close_input();

  if (name) {
    for (uint i = 0; i < midiInGetNumDevs(); i++) {
      MIDIINCAPS caps;

      // get device caps
      if (midiInGetDevCaps(i, &caps, sizeof(caps)))
        continue;

      if (_stricmp(name, caps.szPname) == 0) {
        device_id = i;
        break;
      }
    }
  }

  if (midiInOpen(&midi_in_device, device_id, (DWORD_PTR)&midi_input_callback, 0, CALLBACK_FUNCTION))
    return -1;

  midiInStart(midi_in_device);
  return 0;
}

// close input device
void midi_close_input() {
  thread_lock lock(midi_input_lock);

  if (midi_in_device) {
    midiInClose(midi_in_device);
    midi_in_device = NULL;
  }
}

// enum input
void midi_enum_input(midi_enum_callback &callback) {
  for (uint i = 0; i < midiInGetNumDevs(); i++) {
    MIDIINCAPS caps;

    // get device caps
    if (midiInGetDevCaps(i, &caps, sizeof(caps)))
      continue;

    callback(caps.szPname);
  }
}

// enum input
void midi_enum_output(midi_enum_callback &callback) {
  for (uint i = 0; i < midiOutGetNumDevs(); i++) {
    MIDIOUTCAPS caps;

    // get device caps
    if (midiOutGetDevCaps(i, &caps, sizeof(caps)))
      continue;

    callback(caps.szPname);
  }
}

// midi reset
void midi_reset() {
  // all notes off
  for (int ch = 0; ch < 16; ch++) {
    for (int note = 0; note < 128; note++) {
      if (note_states[ch][note])
        midi_output_event(0x80 | ch, note, 0, 0);
    }

    for (int i = 0; i < 128; i++)
      if (config_get_controller(ch, i) < 128)
        midi_output_event(0xb0 | ch, i, config_get_controller(ch, i), 0);

    if (config_get_program(ch) < 128)
      midi_output_event(0xc0 | ch, config_get_program(ch), 0, 0);
  }
}

// wrap value
static int wrap_value(int value, int min_v, int max_v) {
  if (value < min_v) value = max_v;
  if (value > max_v) value = min_v;
  return value;
}

// clamp value
static int clamp_value(int value, int min_v, int max_v) {
  if (value < min_v) value = min_v;
  if (value > max_v) value = max_v;
  return value;
}

void midi_output_event(byte a, byte b, byte c, byte d) {
  // check note down and note up
  switch (a & 0xf0) {
   // NoteOff
   case 0x80: {
     note_states[a & 0xf][b & 0x7f] = 0;
   }
   break;

   // NoteOn
   case 0x90: {
     note_states[a & 0xf][b & 0x7f] = 1;
   }
   break;

   // Controller
   case 0xb0: {
     int ch = a & 0x0f;
     int id = b & 0x7f;
     int value = config_get_controller(ch, id);

     switch (d) {
      case 0: value = c; break;
      case 1: value = value + (char)c; break;
      case 2: value = value - (char)c; break;
      case 3: value = 127 - value; break;
     }

     value = clamp_value(value, 0, 127);
     config_set_controller(ch, id, value);
     c = value;
     d = 0;
   }
   break;

   // ProgramChange
   case 0xc0: {
     int ch = a & 0x0f;
     int value = config_get_program(ch);

     switch (c) {
      case 0: value = b; break;
      case 1: value = value + (char)b; break;
      case 2: value = value - (char)b; break;
      case 3: value = 127 - value; break;
     }

     value = clamp_value(value, 0, 127);
     config_set_program(ch, value);
     b = value;
     c = 0;
     d = 0;
   }
   break;
  }

  // send midi event to vst plugin
  if (vsti_is_instrument_loaded()) {
    vsti_send_midi_event(a, b, c, d);
  }
  // send event to output device
  else if (midi_out_device) {
    midiOutShortMsg(midi_out_device, a | (b << 8) | (c << 16) | (d << 24));
  }
}

void midi_send_event(byte a, byte b, byte c, byte d) {
  int cmd = a & 0xf0;
  int ch = a & 0x0f;
  byte code = b;

  // note on
  if (cmd == 0x90) {
    // remap channel
    a = cmd | config_get_key_channel(ch);

    // adjust note
    b = clamp_value((int)b + config_get_key_octshift(ch) * 12 + config_get_key_signature(), 0, 127);

    // adjust velocity
    byte vel = config_get_key_velocity(ch);
    if (vel) {
      c = clamp_value((int)c * 127 / vel, 0, 127);
    } else {
      c = 127;
    }

    // auto generate note off event
    midi_keyup_t up;
    up.map.a = 0x80 | (a & 0x0f);
    up.map.b = b;
    up.map.c = c;
    up.map.d = d;
    up.midi_display_key = b;

    if (config_get_midi_display() == MIDI_DISPLAY_INPUT) {
      up.midi_display_key -= config_get_key_signature();
    }

    key_up_map.insert(std::pair<byte, midi_keyup_t>(code, up));

    display_midi_key(up.midi_display_key, true);
    midi_output_event(a, b, c, d);
  }

  // note off
  else if (cmd == 0x80) {
    // display
    if (config_get_midi_display() == MIDI_DISPLAY_INPUT)
      display_midi_key(b, false);

    auto it = key_up_map.find(code);
    while (it != key_up_map.end() && it->first == code) {
      midi_keyup_t &up = it->second;

      if (up.map.a) {
        display_midi_key(up.midi_display_key, false);
        midi_output_event(up.map.a, up.map.b, c, 0);
      }

      ++it;
    }
    key_up_map.erase(code);
  }

  // other events, send directly
  else if (cmd) {
    midi_output_event(a, b, c, d);
  }
}