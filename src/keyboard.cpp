#include "pch.h"
#include <dinput.h>

#include "keyboard.h"
#include "vst/aeffectx.h"
#include "display.h"
#include "config.h"
#include "song.h"
#include "export_mp4.h"

// buffer size

#define DINPUT_BUFFER_SIZE	32

// directinput8 device
static LPDIRECTINPUT8  directinput = NULL;

// directinput8 keyboard
static LPDIRECTINPUTDEVICE8  keyboard = NULL;

// directinput notify event
static HANDLE input_event = NULL;
static HANDLE input_thread = NULL;

// auto generated keyup events
static key_bind_t key_up_event[256];

// original generated keyup event channel
static byte key_up_event_ch[256];

// delay keyup events
static struct delay_keyup_t
{
	byte ch;
	double timer;
	key_bind_t bind;
}
delay_keyup_events[256];

// keyboard enable
static bool enable_keyboard = true;

// keyboard status
static byte keyboard_status[256] = {0};


// -----------------------------------------------------------------------------------------
// accessibility shortcut keys
// -----------------------------------------------------------------------------------------
static void AllowAccessibilityShortcutKeys(bool bAllowKeys, bool bInitialize)
{
	static STICKYKEYS g_StartupStickyKeys = {sizeof(STICKYKEYS), 0};
	static TOGGLEKEYS g_StartupToggleKeys = {sizeof(TOGGLEKEYS), 0};
	static FILTERKEYS g_StartupFilterKeys = {sizeof(FILTERKEYS), 0};

	if (bInitialize)
	{
		SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &g_StartupStickyKeys, 0);
		SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &g_StartupToggleKeys, 0);
		SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &g_StartupFilterKeys, 0);
	}

    if (bAllowKeys)
    {
        // Restore StickyKeys/etc to original state and enable Windows key      
        STICKYKEYS sk = g_StartupStickyKeys;
        TOGGLEKEYS tk = g_StartupToggleKeys;
        FILTERKEYS fk = g_StartupFilterKeys;
        
        SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &g_StartupStickyKeys, 0);
        SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &g_StartupToggleKeys, 0);
        SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &g_StartupFilterKeys, 0);
    }
    else
    {
        // Disable StickyKeys/etc shortcuts but if the accessibility feature is on, 
        // then leave the settings alone as its probably being usefully used
 
        STICKYKEYS skOff = g_StartupStickyKeys;
        if ((skOff.dwFlags & SKF_STICKYKEYSON) == 0)
        {
            // Disable the hotkey and the confirmation
            skOff.dwFlags &= ~SKF_HOTKEYACTIVE;
            skOff.dwFlags &= ~SKF_CONFIRMHOTKEY;
 
            SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &skOff, 0);
        }
 
        TOGGLEKEYS tkOff = g_StartupToggleKeys;
        if ((tkOff.dwFlags & TKF_TOGGLEKEYSON) == 0)
        {
            // Disable the hotkey and the confirmation
            tkOff.dwFlags &= ~TKF_HOTKEYACTIVE;
            tkOff.dwFlags &= ~TKF_CONFIRMHOTKEY;
 
            SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tkOff, 0);
        }
 
        FILTERKEYS fkOff = g_StartupFilterKeys;
        if ((fkOff.dwFlags & FKF_FILTERKEYSON) == 0)
        {
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

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (enable_keyboard && !export_rendering() && nCode == HC_ACTION)
	{
		KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

		uint extent = p->flags & 1;
		uint code = (p->scanCode & 0xFF);
		uint keydown = !(p->flags & LLKHF_UP);

		// special case for scan code
		switch (code)
		{
		case 0x45:	extent = !extent;	break;
		case 0x36:	extent = 0;
		}

		// translate extented key.
		if (extent) code |= 0x80;

		// trigger event only when key changed
		if (keydown_status[code] != keydown)
		{
			song_send_event(SM_SYSTEM, SMS_KEY_EVENT, code, keydown, true);
			keydown_status[code] = keydown;
		}

		if (!config_get_enable_hotkey() || p->vkCode == VK_LWIN || p->vkCode == VK_RWIN)
			return 1;
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// initialize keyboard
int keyboard_init()
{
	// install keyboard hook
	keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL,  LowLevelKeyboardProc, GetModuleHandle(NULL), 0);

	// disable accessibility key
	AllowAccessibilityShortcutKeys(false, true);

	// clear keyup status
	memset(delay_keyup_events, 0, sizeof(delay_keyup_events));

	return 0;
}

// shutdown keyboard system
void keyboard_shutdown()
{
	// enable accessibility key
	AllowAccessibilityShortcutKeys(true, false);

	UnhookWindowsHookEx(keyboard_hook);
	keyboard_hook = NULL;
}


// enable or disable keyboard
void keyboard_enable(bool enable)
{
	enable_keyboard = enable;

	if (!enable)
		keyboard_reset();
}

// reset keyboard
void keyboard_reset()
{
	for (int i = 0; i < ARRAY_COUNT(keyboard_status); i++)
	{
		if (keyboard_status[i] || keydown_status[i])
		{
			// send keyup event
			song_send_event(SM_SYSTEM, SMS_KEY_EVENT, i, 0);
			keyboard_status[i] = 0;
			keydown_status[i] = 0;
		}
	}

	// reset all delay keyup events
	for (int i = 0; i < ARRAY_COUNT(delay_keyup_events); i++)
	{
		if (delay_keyup_events[i].timer > 0)
		{
			key_bind_t bind = delay_keyup_events[i].bind;
			song_send_event(bind.a, bind.b, bind.c, bind.d);
			delay_keyup_events[i].timer = 0;
			delay_keyup_events[i].ch = -1;
		}
	}
}

// enum keymap
void keyboard_enum_keymap(keymap_enum_callback & callback)
{
	char path[256];
	char buff[256];
	config_get_media_path(path, sizeof(path), "");

	_snprintf(buff, sizeof(buff), "%s\\*.map", path);

	WIN32_FIND_DATAA data;
	HANDLE finddata = FindFirstFileA(buff, &data);

	if (finddata != INVALID_HANDLE_VALUE)
	{
		do
		{
			_snprintf(buff, sizeof(buff), "%s\\%s", path, data.cFileName);
			callback(buff);
		}
		while (FindNextFileA(finddata, &data));
	}

	FindClose(finddata);
}

// keyboard event
void keyboard_key_event(int code, int keydown)
{
	// keep state
	if (code < ARRAY_COUNT(keyboard_status))
		keyboard_status[code] = keydown;

	// send keyboard event to display
	display_keyboard_event(code, keydown);

	// translate keyboard event to midi event
	key_bind_t map;
	
	// keydown event
	if (keydown)
	{
		config_get_key_bind(code, &map, NULL);

		// clear keyup event
		key_up_event[code].a = 0;
			
		if (map.a >= 0x80)
		{
			int ch = map.a & 0xf;

			// modify event
			midi_modify_event(map.a, map.b, map.c, map.d, config_get_key_octshift(ch) * 12 + config_get_key_signature(), config_get_key_velocity(ch));

			// remap channel
			map.a = (map.a & 0xf0) | config_get_key_channel(ch);

			// auto generate keyup event
			if ((map.a & 0xf0) == 0x90)
			{
				// reset delay keyup events
				for (int i = 0; i < ARRAY_COUNT(delay_keyup_events); i++)
				{
					if (delay_keyup_events[i].timer > 0 &&
						delay_keyup_events[i].ch == ch &&
						delay_keyup_events[i].bind.b == map.b)
					{
						key_bind_t bind = delay_keyup_events[i].bind;
						song_send_event(bind.a, bind.b, bind.c, bind.d);
						delay_keyup_events[i].timer = 0;
					}
				}

				key_up_event_ch[code] = ch;
				key_up_event[code].a = 0x80 | (map.a & 0x0f);
				key_up_event[code].b = map.b;
				key_up_event[code].c = map.c;
				key_up_event[code].d = map.d;
			}
		}
	}

	// keyup event
	else
	{
		if (key_up_event[code].a)
		{
			int delay_up = config_get_delay_keyup(key_up_event_ch[code]);

			if (delay_up)
			{
				for (int i = 0; i < ARRAY_COUNT(delay_keyup_events); i++)
				{
					if (delay_keyup_events[i].timer <= 0)
					{
						delay_keyup_events[i].timer = 100 * delay_up;
						delay_keyup_events[i].ch = key_up_event_ch[code];
						delay_keyup_events[i].bind = key_up_event[code];
						break;
					}
				}

				// send event to display to correct midi keyup
				display_midi_event(key_up_event[code].a, key_up_event[code].b, key_up_event[code].c, key_up_event[code].d);
			}
			else
			{
				map = key_up_event[code];
			}
		}
		else
			config_get_key_bind(code, NULL, &map);
	}

	// send midi event
	if (map.a)
	{
		// send event to song
		song_send_event(map.a, map.b, map.c, map.d);
	}
}

// keyboard event
void keyboard_update(double time_elapsed)
{
	for (int code = 0; code < ARRAY_COUNT(delay_keyup_events); code++)
	{
		if (delay_keyup_events[code].timer > 0)
		{
			delay_keyup_events[code].timer -= time_elapsed;

			if (delay_keyup_events[code].timer <= 0)
			{
				key_bind_t bind = delay_keyup_events[code].bind;

				// send keyup event
				song_send_event(bind.a, bind.b, bind.c, bind.d);
			}
		}
	}
}