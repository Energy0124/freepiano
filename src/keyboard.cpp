#include "pch.h"
#include <dinput.h>

#include "keyboard.h"
#include "vst/aeffectx.h"
#include "display.h"
#include "config.h"
#include "song.h"

extern AEffect * effect;

// buffer size
#define DINPUT_BUFFER_SIZE	32

// directinput8 device
static LPDIRECTINPUT8  directinput = NULL;

// directinput8 keyboard
static LPDIRECTINPUTDEVICE8  keyboard = NULL;

// directinput notify event
static HANDLE input_event = NULL;
static HANDLE input_thread = NULL;

// key map
static KeyboardEvent key_map[256][2];

// keyboard enable
static bool enable_keyboard = true;
static char oct_shift[16] = {0};
static char key_velocity[16] = {127};
static char key_channel[16] = {0};

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
static HHOOK g_hKeyboardHook = NULL;
static bool g_bWindowActive = false;

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0 || nCode != HC_ACTION)  // do not process message 
        return CallNextHookEx( g_hKeyboardHook, nCode, wParam, lParam); 
 
    bool bEatKeystroke = false;
    KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
    switch (wParam) 
    {
        case WM_KEYDOWN:  
        case WM_KEYUP:    
        {
            bEatKeystroke = (g_bWindowActive && ((p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN)));
            break;
        }
    }
 
    if( bEatKeystroke )
        return 1;
    else
        return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

static void DisableWindowsKey(bool disable)
{
	if (disable)
	{
		if (g_hKeyboardHook == NULL)
		{
			g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL,  LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
		}
	}
	else
	{
		if (g_hKeyboardHook)
		{
			UnhookWindowsHookEx( g_hKeyboardHook );
			g_hKeyboardHook = NULL;
		}
	}
}

// -----------------------------------------------------------------------------------------
// dinput functions
// -----------------------------------------------------------------------------------------
static DWORD __stdcall input_update_thread(void * param)
{
	while (input_thread)
	{
		DIDEVICEOBJECTDATA key_events[DINPUT_BUFFER_SIZE];
		DWORD key_event_count = 0;
		HRESULT hr = E_FAIL;

		// get keyboard state
		if (keyboard && enable_keyboard)
		{
			// get input events
			key_event_count = DINPUT_BUFFER_SIZE;
			hr = keyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), key_events, &key_event_count, 0);

			// retry again
			if (FAILED(hr))
			{
				key_event_count = DINPUT_BUFFER_SIZE;
				hr = keyboard->Acquire();
				if (SUCCEEDED(hr))
					hr = keyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), key_events, &key_event_count, 0);
			}
		}

		// simulate keyup event when keyboard is disabled
		else
		{
			for (int i = 0; i < ARRAY_COUNT(keyboard_status); i++)
			{
				if (keyboard_status[i])
				{
					if (key_event_count < DINPUT_BUFFER_SIZE)
					{
						key_events[key_event_count].dwOfs = i;
						key_events[key_event_count].dwData = 0;
						key_event_count++;
						keyboard_status[i] = 0;
						hr = S_OK;
					}
				}
			}
		}


		if (SUCCEEDED(hr))
		{
			for (uint i = 0; i < key_event_count; i++)
			{
				uint code = key_events[i].dwOfs;
				uint keydown = key_events[i].dwData;

				// send keyboard event
				keyboard_send_event(0, code, keydown, 0);
			}
		}

		// wait event
		WaitForSingleObject(input_event, -1);
	}

	return 0;
}


// initialize keyboard
int keyboard_init()
{
	// create DInput8
	if (FAILED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (void **)&directinput, NULL)))
		goto error;

	// create the keyboard device
	if (FAILED(directinput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL)))
		goto error;

	// set data format
	if (FAILED(keyboard->SetDataFormat(&c_dfDIKeyboard)))
		goto error;

	// set cooperative level
	keyboard->SetCooperativeLevel(NULL, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

	// set property
    DIPROPDWORD prop;
    prop.diph.dwSize = sizeof(DIPROPDWORD);
    prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    prop.diph.dwObj = 0;
    prop.diph.dwHow = DIPH_DEVICE;
    prop.dwData = DINPUT_BUFFER_SIZE;
    keyboard->SetProperty(DIPROP_BUFFERSIZE, &prop.diph);

	// create input event and thread
	input_event = CreateEvent(NULL, false, FALSE, NULL);
	input_thread = CreateThread(NULL, 0, &input_update_thread, NULL, NULL, NULL);

	// set notification event
	HRESULT hr = keyboard->SetEventNotification(input_event);
	if (FAILED(hr))
		goto error;

	// disable accessibility shortcuts 
	AllowAccessibilityShortcutKeys(false, true);
	return 0;

error:
	keyboard_shutdown();
	return -1;
}

// shutdown keyboard system
void keyboard_shutdown()
{
	// enable accessibility shortcuts 
	AllowAccessibilityShortcutKeys(true, false);

	// wait input thread exit
	if (input_thread)
	{
		HANDLE thread = input_thread;
		input_thread = NULL;
		SetEvent(input_event);
		WaitForSingleObject(thread, -1);
		CloseHandle(thread);
	}

	// release keyboard device
	if (keyboard)
	{
		keyboard->Unacquire();
		keyboard->Release();
		keyboard = NULL;
	}

	// release directinput8
	if (directinput)
	{
		directinput->Release();
		directinput = NULL;
	}

	// delete input event
	if (input_event)
	{
		CloseHandle(input_event);
		input_event = NULL;
	}
}


// enable or disable keyboard
void keyboard_enable(bool enable)
{
	enable_keyboard = enable;

	// unacquire keyboard
	if (keyboard)
	{
		if (enable_keyboard)
			keyboard->Acquire();
		else
			keyboard->Unacquire();
	}

	SetEvent(input_event);
}

// keyboard event
static void keyboard_event(int code, int keydown)
{
	song_record_event(0, code, keydown, 0);

	// send keyboard event to display
	display_keyboard_event(code, keydown);

	// translate keyboard event to midi event
	KeyboardEvent map = key_map[code][keydown == 0];

	// send midi event
	if (map.action)
	{
		// modify event
		midi_modify_event(map.action, map.arg1, map.arg2, map.arg3, oct_shift[map.action & 0xf] * 12 + midi_get_key_signature(), key_velocity[map.action & 0xf]);

		// remap channel
		map.action = (map.action & 0xf0) | key_channel[map.action & 0xf];

		// sent event
		midi_send_event(map.action, map.arg1, map.arg2, map.arg3);
	}

	// keep state
	if (code < ARRAY_COUNT(keyboard_status))
		keyboard_status[code] = keydown;
}

// event message
void keyboard_send_event(byte a, byte b, byte c, byte d)
{
	switch (a)
	{
	case 0:	keyboard_event(b, c); break;
	case 1: midi_set_key_signature(b); break;
	case 2: keyboard_set_octshift(b, c); break;
	case 3: keyboard_set_velocity(b, c); break;
	case 4: keyboard_set_channel(b, c); break;
	}
}

// get keyboard map
void keyboard_get_map(byte code, KeyboardEvent * keydown, KeyboardEvent * keyup)
{
	if (keydown) *keydown = key_map[code][0];
	if (keyup) *keyup = key_map[code][1];
}

// set keyboard action
void keyboard_set_map(byte code, KeyboardEvent * keydown, KeyboardEvent * keyup)
{
	if (keydown) key_map[code][0] = *keydown;
	if (keyup) key_map[code][1] = *keyup;
}

// get default keyup
bool keyboard_default_keyup(KeyboardEvent * keydown, KeyboardEvent * keyup)
{
	keyup->action = 0;
	keyup->arg1 = 0;
	keyup->arg2 = 0;
	keyup->arg3 = 0;

	switch (keydown->action & 0xf0)
	{
	case 0x90:
		keyup->action = 0x80 | (keydown->action & 0x0f);
		keyup->arg1 = keydown->arg1;
		keyup->arg2 = keydown->arg2;
		return true;

	case 0xe0:
		keyup->action = keydown->action;
		keyup->arg1 = 0;
		keyup->arg2 = 0;
		return true;
	};

	return false;
}

// set oct shift
void keyboard_set_octshift(byte channel, char shift)
{
	if (channel < ARRAY_COUNT(oct_shift))
		oct_shift[channel] = shift;
}

// get oct shift
char keyboard_get_octshift(byte channel)
{
	if (channel < ARRAY_COUNT(oct_shift))
		return oct_shift[channel];
	else
		return 0;
}

// set velocity
void keyboard_set_velocity(byte channel, byte velocity)
{
	if (channel < ARRAY_COUNT(key_velocity))
		key_velocity[channel] = velocity;
}

// get velocity
byte keyboard_get_velocity(byte channel)
{
	if (channel < ARRAY_COUNT(key_velocity))
		return key_velocity[channel];
	else
		return 0;
}

// get channel
int keyboard_get_channel(byte channel)
{
	if (channel < ARRAY_COUNT(key_channel))
		return key_channel[channel];
	else
		return 0;
}

// get channel
void keyboard_set_channel(byte channel, byte value)
{
	if (channel < ARRAY_COUNT(key_channel))
		key_channel[channel] = value;
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


