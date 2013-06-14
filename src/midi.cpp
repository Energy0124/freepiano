#include "pch.h"
#include "midi.h"
#include "synthesizer_vst.h"
#include "display.h"
#include "song.h"
#include "config.h"

#include <map>
#include <string>

// midi input devices
struct midi_in_device_t {
  bool enable;
  HMIDIIN device;
  int remap;

  midi_in_device_t() 
    : enable(false)
    , device(NULL)
    , remap(0)
  {
  }
};

static std::map<std::string, midi_in_device_t> midi_inputs;
static HMIDIOUT midi_out_device = NULL;

// midi thread lock
static thread_lock_t midi_input_lock;
static thread_lock_t midi_output_lock;

// note state
static byte note_states[16][128] = {0};


// controller save state
static struct controller_state_t {
  double restore_timer;
  byte   restore_value;
  bool sync_wait;
  byte sync_value;
}
midi_controller_state[16][128] = {0};
static bool midi_sync_trigger = false;

// auto generated keyup events
struct midi_keyup_t {
  byte midi_display_key;
  key_bind_t map;
};
static std::multimap<byte, midi_keyup_t> key_up_map;

// get key status
byte midi_get_note_status(byte ch, byte note) {
    return note_states[ch & 0x0f][note & 0x7f];
}

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

static inline byte clamp_value(int data, byte min = 0, byte max = 127) {
  if (data < min) return min;
  if (data > max) return max;
  return data;
}

static void CALLBACK midi_input_callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
  if (wMsg == MIM_DATA) {
    uint data = (uint)dwParam1;
    byte a = data >> 0;
    byte b = data >> 8;
    byte c = data >> 16;
    byte d = data >> 24;

    midi_in_device_t *device = (midi_in_device_t*)dwInstance;

    // remap channel
    if (device->remap >= 1 && device->remap <= 16) {
      byte op = a & 0xf0;
      byte ch = config_get_key_channel(device->remap - 1);

      a = op | ch;

      if (op == 0x80 || op == 0x90) {
        if (config_get_midi_transpose())
          b = clamp_value((int)b + config_get_key_signature());
      }
    }

    song_send_event(a, b, c, d, true);
  }
}

// open input device
void midi_open_inputs() {
  thread_lock lock(midi_input_lock);

  // clear flags
  for (auto it = midi_inputs.begin(); it != midi_inputs.end(); ++it) {
    it->second.enable = false;
  }

  // enum devices
  for (uint i = 0; i < midiInGetNumDevs(); i++) {
    midi_input_config_t config;
    MIDIINCAPS caps;

    // get device caps
    if (midiInGetDevCaps(i, &caps, sizeof(caps)))
      continue;

    config_get_midi_input_config(caps.szPname, &config);

    if (config.enable) {
      std::string key(caps.szPname);
      midi_in_device_t &input = midi_inputs[key];
      input.enable = true;
      input.remap = config.remap;

      if (input.device == NULL) {
        if (midiInOpen(&input.device, i, (DWORD_PTR)&midi_input_callback, (DWORD_PTR)&input, CALLBACK_FUNCTION)) {
          input.enable = false;
          input.device = NULL;
        }
        midiInStart(input.device);
      }
    }
  }

  // close removed devices
  for (auto it = midi_inputs.begin(); it != midi_inputs.end(); ) {
    auto prev = it;
    ++it;

    if (!prev->second.enable) {
      midiInStop(prev->second.device);
      midiInClose(prev->second.device);

      midi_inputs.erase(prev);
    }
  }

  return;
}

// close input device
void midi_close_inputs() {
  thread_lock lock(midi_input_lock);

  for (auto it = midi_inputs.begin(); it != midi_inputs.end(); ++it) {
    midiInStop(it->second.device);
    midiInClose(it->second.device);
  }
  midi_inputs.clear();
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

  // clear controller save
  memset(midi_controller_state, 0, sizeof(midi_controller_state));

  // clear state
  midi_sync_trigger = false;
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
     midi_sync_trigger = true;
   }
   break;

   // Controller
   case 0xb0: {
     int ch = a & 0x0f;
     int id = b & 0x7f;
     int value = config_get_controller(ch, id);

     // midi controller state
     controller_state_t &state = midi_controller_state[ch][id];

     // action
     switch (d & 0x0f) {
      case 0: value = c; break;
      case 1: value = value + (char)c; break;
      case 2: value = value - (char)c; break;
      case 3: value = 127 - value; break;
      case 4: state.restore_timer = 20;
              state.restore_value = value;
              value = c;
              break;
      case 10: value = c / 10 * 10 + (value % 10); break;
      case 11: value = value / 10 * 10 + (c % 10); break;
     }

     // clamp value
     value = clamp_value(value, 0, 127);

     // sync
     if (d & 0x10) {
       state.sync_wait = true;
       state.sync_value = value;
       return;
     }

     config_set_controller(ch, id, value);

     c = value;
     d = 0;
   }
   break;

   // ProgramChange
   case 0xc0: {
     int ch = a & 0x0f;
     int value = config_get_program(ch);
     if (value > 127) value = 0;

     switch (c) {
      case 0: value = b; break;
      case 1: value = value + (char)b; break;
      case 2: value = value - (char)b; break;
      case 3: value = 127 - value; break;
      case 10: value = b / 10 * 10 + (value % 10); break;
      case 11: value = value / 10 * 10 + (b % 10); break;
     }

     value = clamp_value(value, 0, 127);
     config_set_program(ch, value);
     b = value;
     c = 0;
     d = 0;
   }
   break;
  }

  // debug print
#ifdef _DEBUG
  fprintf(stdout, "MIDI OUT: %04x %02x %02x %02x %02x\n", GetTickCount(), a, b, c, d);
#endif

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

  // BUG fix: some keyboard send note on with a small velocity when key up.
  if (cmd == 0x90 && c < 5) {
    cmd = 0x80;
  }

  // note on
  if (cmd == 0x90) {
    // remap channel
    a = cmd | config_get_key_channel(ch);

    // adjust note
    b = clamp_value((int)b + config_get_key_octshift(ch) * 12 + config_get_key_transpose(ch) + config_get_key_signature(), 0, 127);

    // auto generate note off event
    midi_keyup_t up;
    up.map.a = 0x80 | (a & 0x0f);
    up.map.b = b;
    up.map.c = c;
    up.map.d = d;
    up.midi_display_key = b;

    if (config_get_midi_transpose()) {
      up.midi_display_key -= config_get_key_signature();
    }

    key_up_map.insert(std::pair<byte, midi_keyup_t>(code, up));
    midi_output_event(a, b, c, d);
  }

  // note off
  else if (cmd == 0x80) {
    auto it = key_up_map.find(code);
    while (it != key_up_map.end() && it->first == code) {
      midi_keyup_t &up = it->second;

      if (up.map.a) {
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

// update midi
void midi_update(double time) {
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
        state.restore_timer -= time;

        if (state.restore_timer <= 0) {
          midi_output_event(0xb0 | ch, id, state.restore_value, 0);
        }
      }
    }
  }

  midi_sync_trigger = false;
}
