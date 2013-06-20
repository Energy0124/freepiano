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

      if (op == SM_MIDI_NOTEOFF || op == SM_MIDI_NOTEON) {
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
        midi_output_event(SM_MIDI_NOTEOFF | ch, note, 0, 0);
    }

    for (int i = 0; i < 128; i++)
      if (config_get_controller(ch, i) < 128)
        midi_output_event(SM_MIDI_CONTROLLER | ch, i, config_get_controller(ch, i), 0);

    if (config_get_program(ch) < 128)
      midi_output_event(SM_MIDI_PROGRAM | ch, config_get_program(ch), 0, 0);
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
  // note on with a small velocity as note off.
  if ((a & 0xf0) == SM_MIDI_NOTEON) {
    if (c < 5) {
      a = SM_MIDI_NOTEOFF | (a & 0x0f);
      c = 0;
    }
  }

  // check note down and note up
  switch (a & 0xf0) {
   case SM_MIDI_NOTEOFF: {
     note_states[a & 0xf][b & 0x7f] = 0;
   }
   break;

   case SM_MIDI_NOTEON: {
     note_states[a & 0xf][b & 0x7f] = 1;
     song_trigger_sync(SONG_SYNC_FLAG_MIDI);
   }
   break;

   case SM_MIDI_CONTROLLER: {
     config_set_controller(a & 0x0f, b & 0x7f, c);
     d = 0;
   }
   break;

   case SM_MIDI_PROGRAM: {
     config_set_program(a & 0x0f, b);
     c = 0;
     d = 0;
   }
   break;

   case SM_MIDI_PITCH_BEND:
     config_set_pitchbend(a & 0x0f, (int)c - 64);
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
  else {
    thread_lock lock(midi_output_lock);
    if (midi_out_device) {
      midiOutShortMsg(midi_out_device, a | (b << 8) | (c << 16) | (d << 24));
    }
  }
}