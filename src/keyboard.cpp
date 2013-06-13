#include "pch.h"
#include <dinput.h>

#include "keyboard.h"
#include "vst/aeffectx.h"
#include "display.h"
#include "config.h"
#include "song.h"
#include "export.h"

#include <map>

// buffer size

#define DINPUT_BUFFER_SIZE  32

// directinput8 device
static LPDIRECTINPUT8 directinput = NULL;

// directinput8 keyboard
static LPDIRECTINPUTDEVICE8 keyboard = NULL;

// directinput notify event
static HANDLE input_event = NULL;
static HANDLE input_thread = NULL;

// auto generated keyup events
struct keyup_t {
  byte delay_channel;
  byte midi_display_key;
  key_bind_t map;
};
static std::multimap<byte, keyup_t> key_up_map;


// keyboard status
static byte keyboard_status[256] = {0};

// -----------------------------------------------------------------------------------------
// delay keyup
// -----------------------------------------------------------------------------------------
static struct delay_keyup_t {
  double timer;
  byte channel;
  key_bind_t map;
}
delay_keyup_events[128 * 16];

static void delay_keyup_init() {
  for (int i = 0; i < ARRAY_COUNT(delay_keyup_events); i++) {
    delay_keyup_events[i].timer = -1;
  }
}

static void delay_keyup_reset(int id) {
  delay_keyup_t &delay = delay_keyup_events[id];

  if (delay.timer >= 0) {
    if (delay.map.a) {
      song_output_event(delay.map.a, delay.map.b, delay.map.c, delay.map.d);
    }
    delay.timer = -1;
  }
}

static void delay_keyup_add(int id, key_bind_t bind) {
  delay_keyup_events[id].timer = 0;
  delay_keyup_events[id].map = bind;
}

static void delay_keyup_update(double time_elapsed) {
  for (int i = 0; i < ARRAY_COUNT(delay_keyup_events); i++) {
    delay_keyup_t &delay = delay_keyup_events[i];

    if (delay.timer >= 0) {
      delay.timer += time_elapsed;
      if (delay.timer > config_get_delay_keyup(i / 128) * 100) {
        delay_keyup_reset(i);
      }
    }
  }
}

// -----------------------------------------------------------------------------------------
// accessibility shortcut keys
// -----------------------------------------------------------------------------------------
static void AllowAccessibilityShortcutKeys(bool bAllowKeys, bool bInitialize) {
  static STICKYKEYS g_StartupStickyKeys = {sizeof(STICKYKEYS), 0};
  static TOGGLEKEYS g_StartupToggleKeys = {sizeof(TOGGLEKEYS), 0};
  static FILTERKEYS g_StartupFilterKeys = {sizeof(FILTERKEYS), 0};

  if (bInitialize) {
    SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &g_StartupStickyKeys, 0);
    SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &g_StartupToggleKeys, 0);
    SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &g_StartupFilterKeys, 0);
  }

  if (bAllowKeys) {
    // Restore StickyKeys/etc to original state and enable Windows key
    STICKYKEYS sk = g_StartupStickyKeys;
    TOGGLEKEYS tk = g_StartupToggleKeys;
    FILTERKEYS fk = g_StartupFilterKeys;

    SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &g_StartupStickyKeys, 0);
    SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &g_StartupToggleKeys, 0);
    SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &g_StartupFilterKeys, 0);
  } else {
    // Disable StickyKeys/etc shortcuts but if the accessibility feature is on,
    // then leave the settings alone as its probably being usefully used

    STICKYKEYS skOff = g_StartupStickyKeys;
    if ((skOff.dwFlags & SKF_STICKYKEYSON) == 0) {
      // Disable the hotkey and the confirmation
      skOff.dwFlags &= ~SKF_HOTKEYACTIVE;
      skOff.dwFlags &= ~SKF_CONFIRMHOTKEY;

      SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &skOff, 0);
    }

    TOGGLEKEYS tkOff = g_StartupToggleKeys;
    if ((tkOff.dwFlags & TKF_TOGGLEKEYSON) == 0) {
      // Disable the hotkey and the confirmation
      tkOff.dwFlags &= ~TKF_HOTKEYACTIVE;
      tkOff.dwFlags &= ~TKF_CONFIRMHOTKEY;

      SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tkOff, 0);
    }

    FILTERKEYS fkOff = g_StartupFilterKeys;
    if ((fkOff.dwFlags & FKF_FILTERKEYSON) == 0) {
      // Disable the hotkey and the confirmation
      fkOff.dwFlags &= ~FKF_HOTKEYACTIVE;
      fkOff.dwFlags &= ~FKF_CONFIRMHOTKEY;

      SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &fkOff, 0);
    }
  }
}

// -----------------------------------------------------------------------------------------
// disable windows key (works only in dll)
// -----------------------------------------------------------------------------------------
static HHOOK keyboard_hook = NULL;
static byte keydown_status[256] = {0};

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HC_ACTION && !export_rendering()) {
    KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;

    uint extent = p->flags & 1;
    uint code = (p->scanCode & 0xFF);
    uint keydown = !(p->flags & LLKHF_UP);

    // special case for scan code
    switch (code) {
     case 0x45:  extent = !extent;   break;
     case 0x36:  extent = 0;
    }

    // translate extented key.
    if (extent) code |= 0x80;

    // trigger event only when key changed
    if (keydown_status[code] != keydown) {
      song_send_event(SM_SYSTEM, SMS_KEY_EVENT, code, keydown, true);
      keydown_status[code] = keydown;
    }

    if (!config_get_enable_hotkey() || p->vkCode == VK_LWIN || p->vkCode == VK_RWIN)
      return 1;
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// initialize keyboard
int keyboard_init() {
  // disable accessibility key
  AllowAccessibilityShortcutKeys(false, true);

  // clear keyup status
  delay_keyup_init();

  return 0;
}

// shutdown keyboard system
void keyboard_shutdown() {
  // enable accessibility key
  AllowAccessibilityShortcutKeys(true, false);

  // disable keyboard hook
  keyboard_enable(false);
}


// enable or disable keyboard
void keyboard_enable(bool enable) {
  if (enable) {
    // install keyboard hook
    if (keyboard_hook == NULL) {
      keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL,  LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    }
  }
  else {
    // uninstall keyboard hook
    if (keyboard_hook) {
      UnhookWindowsHookEx(keyboard_hook);
      keyboard_hook = NULL;
    }

    keyboard_reset();
  }
}

// reset keyboard
void keyboard_reset() {
  for (int i = 0; i < ARRAY_COUNT(keyboard_status); i++) {
    if (keyboard_status[i] || keydown_status[i]) {
      // send keyup event
      keyboard_send_event(i, 0);
      keyboard_status[i] = 0;
      keydown_status[i] = 0;
    }
  }

  // reset all delay keyup events
  for (int i = 0; i < ARRAY_COUNT(delay_keyup_events); i++) {
    delay_keyup_reset(i);
  }
}

// enum keymap
void keyboard_enum_keymap(keymap_enum_callback &callback) {
  char path[256];
  char buff[256];
  config_get_media_path(path, sizeof(path), "");

  _snprintf(buff, sizeof(buff), "%s\\*.map", path);

  WIN32_FIND_DATAA data;
  HANDLE finddata = FindFirstFileA(buff, &data);

  if (finddata != INVALID_HANDLE_VALUE) {
    do {
      _snprintf(buff, sizeof(buff), "%s\\%s", path, data.cFileName);
      callback(buff);
    } while (FindNextFileA(finddata, &data));
  }

  FindClose(finddata);
}

static inline byte clamp_value(int data, byte min = 0, byte max = 127) {
  if (data < min) return min;
  if (data > max) return max;
  return data;
}

static inline void modify_midi_event(key_bind_t &map) {
  int cmd = map.a & 0xf0;
  int ch = map.a & 0x0f;

  if (cmd == 0x80 || cmd == 0x90) {
    // remap channel
    map.a = cmd | config_get_key_channel(ch);

    // adjust note
    map.b = clamp_value((int)map.b + config_get_key_octshift(ch) * 12 + config_get_key_transpose(ch) + config_get_key_signature());

    // adjust velocity
    map.c = clamp_value((int)map.c * config_get_key_velocity(ch) / 127);
  }
}

// keyboard event
void keyboard_send_event(int code, int keydown) {
  // keep state
  if (code < ARRAY_COUNT(keyboard_status))
    keyboard_status[code] = keydown;

  // send keyboard event to display
  display_keyboard_event(code, keydown);

  // keydown event
  if (keydown) {
    key_bind_t temp[256];

    // get key bind
    int num_keydown = config_bind_get_keydown(code, temp, ARRAYSIZE(temp));
    for (int i = 0; i < num_keydown; i++) {
      key_bind_t down = temp[i];

      // note on
      if ((down.a & 0xf0) == 0x90) {
        byte ch = down.a & 0x0f;

        // auto generate note off event
        keyup_t up;

        // reset delay keyup event
        delay_keyup_reset(ch * 128 + down.b);

        // translate to real midi event
        modify_midi_event(down);

        // display midi event
        up.midi_display_key = down.b;
        if (config_get_midi_display() == MIDI_DISPLAY_INPUT)
          up.midi_display_key -= config_get_key_signature();

        // display midi event
        display_midi_key(up.midi_display_key, true);

        up.delay_channel = ch;
        up.map.a = 0x80 | (down.a & 0x0f);
        up.map.b = down.b;
        up.map.c = down.c;
        up.map.d = down.d;
        key_up_map.insert(std::pair<byte, keyup_t>(code, up));

      }

      // send event to song
      if (down.a) {
        song_output_event(down.a, down.b, down.c, down.d);
      }
    }

    // add key up events.
    int num_keyup = config_bind_get_keyup(code, temp, ARRAYSIZE(temp));
    for (int i = 0; i < num_keyup; i++) {
      keyup_t up;
      up.map = temp[i];

      // translate to real midi event
      modify_midi_event(up.map);

      // insert keyup event to keyup map
      key_up_map.insert(std::pair<byte, keyup_t>(code, up));
    }
  }
  // keyup event
  else {
    auto it = key_up_map.find(code);
    while (it != key_up_map.end() && it->first == code) {
      keyup_t &up = it->second;

      if (up.map.a) {
        // delay keyup is enabled and this is a note off event.
        if ((up.map.a & 0xf0) == 0x80 && config_get_delay_keyup(up.delay_channel)) {
          // add event to delay keyup queue.
          delay_keyup_add(up.delay_channel * 128 + up.map.b, up.map);

          // send event to display to correct midi keyup
          if (config_get_midi_display() == MIDI_DISPLAY_OUTPUT) {
            display_midi_key(up.map.b, false);
          }
        }

        // send keyup event
        else {
          display_midi_key(up.midi_display_key, false);
          song_output_event(up.map.a, up.map.b, up.map.c, up.map.d);
        }
      }

      ++it;
    }
    key_up_map.erase(code);
  }
}

// keyboard event
void keyboard_update(double time_elapsed) {
  delay_keyup_update(time_elapsed);
}