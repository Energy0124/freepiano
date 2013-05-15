#include "pch.h"
#include <dinput.h>
#include <Shlwapi.h>

#include "config.h"
#include "stdio.h"
#include "keyboard.h"
#include "output_asio.h"
#include "output_dsound.h"
#include "output_wasapi.h"
#include "synthesizer_vst.h"
#include "display.h"
#include "song.h"
#include "gui.h"

#include "../res/resource.h"



// -----------------------------------------------------------------------------------------
// config constants
// -----------------------------------------------------------------------------------------
struct name_t
{
	const char * name;
	uint value;
};

static name_t key_names[] = 
{
	{ "Esc",			DIK_ESCAPE },
	{ "F1",				DIK_F1 },
	{ "F2",				DIK_F2 },
	{ "F3",				DIK_F3 },
	{ "F4",				DIK_F4 },
	{ "F5",				DIK_F5 },
	{ "F6",				DIK_F6 },
	{ "F7",				DIK_F7 },
	{ "F8",				DIK_F8 },
	{ "F9",				DIK_F9 },
	{ "F10",			DIK_F10 },
	{ "F11",			DIK_F11 },
	{ "F12",			DIK_F12 },
	{ "~",				DIK_GRAVE },
	{ "`",				DIK_GRAVE },
	{ "1",				DIK_1 },
	{ "2",				DIK_2 },
	{ "3",				DIK_3 },
	{ "4",				DIK_4 },
	{ "5",				DIK_5 },
	{ "6",				DIK_6 },
	{ "7",				DIK_7 },
	{ "8",				DIK_8 },
	{ "9",				DIK_9 },
	{ "0",				DIK_0 },
	{ "-",				DIK_MINUS },
	{ "=",				DIK_EQUALS },
	{ "Backspace",		DIK_BACK },
	{ "Back",			DIK_BACK },
	{ "Tab",			DIK_TAB },
	{ "Q",				DIK_Q },
	{ "W",				DIK_W },
	{ "E",				DIK_E },
	{ "R",				DIK_R },
	{ "T",				DIK_T },
	{ "Y",				DIK_Y },
	{ "U",				DIK_U },
	{ "I",				DIK_I },
	{ "O",				DIK_O },
	{ "P",				DIK_P },
	{ "{",				DIK_LBRACKET },
	{ "[",				DIK_LBRACKET },
	{ "}",				DIK_RBRACKET },
	{ "]",				DIK_RBRACKET },
	{ "|",				DIK_BACKSLASH },
	{ "\\",				DIK_BACKSLASH },
	{ "CapsLock",		DIK_CAPITAL },
	{ "Caps",			DIK_CAPITAL },
	{ "A",				DIK_A },
	{ "S",				DIK_S },
	{ "D",				DIK_D },
	{ "F",				DIK_F },
	{ "G",				DIK_G },
	{ "H",				DIK_H },
	{ "J",				DIK_J },
	{ "K",				DIK_K },
	{ "L",				DIK_L },
	{ ":",				DIK_SEMICOLON },
	{ ";",				DIK_SEMICOLON },
	{ "\"",				DIK_APOSTROPHE },
	{ "'",				DIK_APOSTROPHE },
	{ "Enter",			DIK_RETURN },
	{ "Shift",			DIK_LSHIFT },
	{ "Z",				DIK_Z },
	{ "X",				DIK_X },
	{ "C",				DIK_C },
	{ "V",				DIK_V },
	{ "B",				DIK_B },
	{ "N",				DIK_N },
	{ "M",				DIK_M },
	{ "<",				DIK_COMMA },
	{ ",",				DIK_COMMA },
	{ ">",				DIK_PERIOD },
	{ ".",				DIK_PERIOD },
	{ "?",				DIK_SLASH },
	{ "/",				DIK_SLASH },
	{ "RShift",			DIK_RSHIFT },
	{ "Ctrl",			DIK_LCONTROL },
	{ "Win",			DIK_LWIN },
	{ "Alt",			DIK_LMENU },
	{ "Space",			DIK_SPACE },
	{ "Apps",			DIK_APPS },
	{ "RAlt",			DIK_RMENU },
	{ "RCtrl",			DIK_RCONTROL },
	  
	{ "Num0",			DIK_NUMPAD0 },
	{ "Num.",			DIK_NUMPADPERIOD },
	{ "NumEnter",		DIK_NUMPADENTER },
	{ "Num1",			DIK_NUMPAD1 },
	{ "Num2",			DIK_NUMPAD2 },
	{ "Num3",			DIK_NUMPAD3 },
	{ "Num4",			DIK_NUMPAD4 },
	{ "Num5",			DIK_NUMPAD5 },
	{ "Num6",			DIK_NUMPAD6 },
	{ "Num7",			DIK_NUMPAD7 },
	{ "Num8",			DIK_NUMPAD8 },
	{ "Num9",			DIK_NUMPAD9 },
	{ "Num+",			DIK_NUMPADPLUS },
	{ "NumLock",		DIK_NUMLOCK },
	{ "Num/",			DIK_NUMPADSLASH },
	{ "Num*",			DIK_NUMPADSTAR },
	{ "Num-",			DIK_NUMPADMINUS },
	  
	{ "Insert",			DIK_INSERT },
	{ "Home",			DIK_HOME },
	{ "PgUp",			DIK_PGUP },
	{ "PageUp",			DIK_PGUP },
	{ "Delete",			DIK_DELETE },
	{ "End",			DIK_END },
	{ "PgDn",			DIK_PGDN },
	{ "PgDown",			DIK_PGDN },
	  
	{ "Up",				DIK_UP },
	{ "Left",			DIK_LEFT },
	{ "Down",			DIK_DOWN },
	{ "Right",			DIK_RIGHT },
	  
	{ "PrintScreen",	DIK_SYSRQ },
	{ "SysRq",			DIK_SYSRQ },
	{ "ScrollLock",		DIK_SCROLL },
	{ "Pause",			DIK_PAUSE },
	{ "Break",			DIK_PAUSE },
};

static name_t action_names[] = 
{
	{ "KeySignature",		SM_KEY_SIGNATURE },
	{ "Octshift",			SM_OCTSHIFT },
	{ "KeyboardOctshift",	SM_OCTSHIFT },
	{ "Velocity",			SM_VELOCITY },
	{ "KeyboardVeolcity",	SM_VELOCITY },
	{ "Channel",			SM_CHANNEL },
	{ "KeyboardChannel",	SM_CHANNEL },
	{ "Volume",				SM_VOLUME },
	{ "Play",				SM_PLAY },
	{ "Record",				SM_RECORD },
	{ "Stop",				SM_STOP },
	{ "Group",				SM_SETTING_GROUP },
	{ "GroupCount",			SM_SETTING_GROUP_COUNT },
	{ "AutoPedal",			SM_AUTO_PEDAL },
	{ "DelayKeyup",			SM_DELAY_KEYUP },


	{ "NoteOff",			0x80 },
	{ "NoteOn",				0x90 },
	{ "KeyPressure",		0xa0 },
	{ "Controller",			0xb0 },
	{ "Program",			0xc0 },
	{ "ChannelPressure",	0xd0 },
	{ "PitchBend",			0xe0 },
};

static name_t note_names[] = 
{
	{ "C0",					0 },
	{ "C#0",				1 },
	{ "D0",					2 },
	{ "D#0",				3 },
	{ "E0",					4 },
	{ "F0",					5 },
	{ "F#0",				6 },
	{ "G0",					7 },
	{ "G#0",				8 },
	{ "A0",					9 },
	{ "A#0",				10 },
	{ "B0",					11 },
	{ "C1",					12 },
	{ "C#1",				13 },
	{ "D1",					14 },
	{ "D#1",				15 },
	{ "E1",					16 },
	{ "F1",					17 },
	{ "F#1",				18 },
	{ "G1",					19 },
	{ "G#1",				20 },
	{ "A1",					21 },
	{ "A#1",				22 },
	{ "B1",					23 },
	{ "C2",					24 },
	{ "C#2",				25 },
	{ "D2",					26 },
	{ "D#2",				27 },
	{ "E2",					28 },
	{ "F2",					29 },
	{ "F#2",				30 },
	{ "G2",					31 },
	{ "G#2",				32 },
	{ "A2",					33 },
	{ "A#2",				34 },
	{ "B2",					35 },
	{ "C3",					36 },
	{ "C#3",				37 },
	{ "D3",					38 },
	{ "D#3",				39 },
	{ "E3",					40 },
	{ "F3",					41 },
	{ "F#3",				42 },
	{ "G3",					43 },
	{ "G#3",				44 },
	{ "A3",					45 },
	{ "A#3",				46 },
	{ "B3",					47 },
	{ "C4",					48 },
	{ "C#4",				49 },
	{ "D4",					50 },
	{ "D#4",				51 },
	{ "E4",					52 },
	{ "F4",					53 },
	{ "F#4",				54 },
	{ "G4",					55 },
	{ "G#4",				56 },
	{ "A4",					57 },
	{ "A#4",				58 },
	{ "B4",					59 },
	{ "C5",					60 },
	{ "C#5",				61 },
	{ "D5",					62 },
	{ "D#5",				63 },
	{ "E5",					64 },
	{ "F5",					65 },
	{ "F#5",				66 },
	{ "G5",					67 },
	{ "G#5",				68 },
	{ "A5",					69 },
	{ "A#5",				70 },
	{ "B5",					71 },
	{ "C6",					72 },
	{ "C#6",				73 },
	{ "D6",					74 },
	{ "D#6",				75 },
	{ "E6",					76 },
	{ "F6",					77 },
	{ "F#6",				78 },
	{ "G6",					79 },
	{ "G#6",				80 },
	{ "A6",					81 },
	{ "A#6",				82 },
	{ "B6",					83 },
	{ "C7",					84 },
	{ "C#7",				85 },
	{ "D7",					86 },
	{ "D#7",				87 },
	{ "E7",					88 },
	{ "F7",					89 },
	{ "F#7",				90 },
	{ "G7",					91 },
	{ "G#7",				92 },
	{ "A7",					93 },
	{ "A#7",				94 },
	{ "B7",					95 },
	{ "C8",					96 },
	{ "C#8",				97 },
	{ "D8",					98 },
	{ "D#8",				99 },
	{ "E8",					100 },
	{ "F8",					101 },
	{ "F#8",				102 },
	{ "G8",					103 },
	{ "G#8",				104 },
	{ "A8",					105 },
	{ "A#8",				106 },
	{ "B8",					107 },
	{ "C9",					108 },
	{ "C#9",				109 },
	{ "D9",					110 },
	{ "D#9",				111 },
	{ "E9",					112 },
	{ "F9",					113 },
	{ "F#9",				114 },
	{ "G9",					115 },
	{ "G#9",				116 },
	{ "A9",					117 },
	{ "A#9",				118 },
	{ "B9",					119 },
};

static name_t controller_names[] = 
{
	{ "BankSelect",			0x0 },
	{ "Modulation",			0x1 },
	{ "BreathControl",		0x2 },
	{ "FootPedal",			0x4 },
	{ "Portamento",			0x5 },
	{ "DataEntry",			0x6 },
	{ "MainVolume",			0x7 },
	{ "Balance",			0x8 },
	{ "Pan",				0xa },
	{ "Expression",			0xb },
	{ "EffectSelector1",	0xc },
	{ "EffectSelector2",	0xd },

	{ "GeneralPurpose1",	0x10 },
	{ "GeneralPurpose2",	0x11 },
	{ "GeneralPurpose3",	0x12 },
	{ "GeneralPurpose4",	0x13 },

	{ "SustainPedal",		0x40 },
	{ "PortamentoPedal",	0x41 },
	{ "SostenutoPedal",		0x42 },
	{ "SoftPedal",			0x43 },
	{ "LegatoPedal",		0x44 },
	{ "Hold2",				0x45 },
	{ "SoundController1",	0x46 },
	{ "SoundController2",	0x47 },
	{ "SoundController3",	0x48 },
	{ "SoundController4",	0x49 },
	{ "SoundController5",	0x4a },
	{ "SoundController6",	0x4b },
	{ "SoundController7",	0x4c },
	{ "SoundController8",	0x4d },
	{ "SoundController9",	0x4e },
	{ "SoundController10",	0x4f },

	{ "DataIncrement",		0x60 },
	{ "DataDecrement",		0x61 },
	{ "NRPNLSB",			0x62 },
	{ "NRPNMSB",			0x63 },
	{ "RPNLSB",				0x64 },
	{ "RPNMSB",				0x65 },

	{ "AllSoundsOff",		0x78 },
	{ "ResetAllControllers",0x79 },
	{ "LocalControlOnOff",	0x7a },
	{ "AllNotesOff",		0x7b },
	{ "OmniModeOff",		0x7c },
	{ "OmniModeOn",			0x7d },
	{ "MonoModeOn",			0x7e },
	{ "PokyModeOn",			0x7f },
};

static name_t value_action_names[] = 
{
	{ "Set",		0 },
	{ "Inc",		1 },
	{ "Dec",		2 },
	{ "Flip",		3 },
	{ "Set10",		10 },
	{ "Set1",		11 },
};

static name_t instrument_type_names[] = 
{
	{ "None",			INSTRUMENT_TYPE_NONE },
	{ "VSTI",			INSTRUMENT_TYPE_VSTI },
};

static name_t output_type_names[] = 
{
	{ "Auto",			OUTPUT_TYPE_AUTO },
	{ "DirectSound",	OUTPUT_TYPE_DSOUND },
	{ "Wasapi",			OUTPUT_TYPE_WASAPI },
	{ "ASIO",			OUTPUT_TYPE_ASIO },
};

static name_t boolean_names[] = 
{
	{ "disable",		0 },
	{ "no",				0 },
	{ "false",			0 },
	{ "enable",			1 },
	{ "yes",			1 },
	{ "true",			1 },
};


// -----------------------------------------------------------------------------------------
// configurations
// -----------------------------------------------------------------------------------------
struct key_label_t
{
	char text[16];
};

struct global_setting_t
{
	uint instrument_type;
	char instrument_path[256];
	uint output_type;
	char output_device[256];
	uint output_delay;
	char keymap[256];
	char midi_input[256];
	char midi_output[256];
	uint output_volume;

	uint enable_hotkey;
	uint enable_resize;
};

struct setting_t
{
	// key map
	key_bind_t key_map[256][2];
	key_label_t key_label[256];

	// key properties
	char key_octshift[16];
	char key_velocity[16];
	char key_channel[16];
	char key_signature;
	char auto_pedal;
	char delay_keyup[16];
	char midi_program[16];
	char midi_controller[16][256];

	void clear()
	{
		memset(this, 0, sizeof(setting_t));

		for (int i = 0; i < 16; i++)
		{
			key_velocity[i] = 127;

			midi_program[i] = -1;

			for (int j = 0; j < 128; j++)
				midi_controller[i][j] = -1;
		}
	}
};

// settings
static global_setting_t global;
static setting_t settings[16];
static uint current_setting = 0;
static uint setting_count = 1;

// save current key settings
static int config_save_key_settings(char * buff, int buffer_size);

// clear keyboard setting
void config_clear_key_setting()
{
	settings[current_setting].clear();
}

// copy key setting
void config_copy_key_setting()
{
	if (OpenClipboard(NULL))
	{
		// temp buffer
		uint buff_size = 1024 * 1024;
		char * buff = new char[buff_size];

		// save key settings
		int size = config_save_key_settings(buff, buff_size);

		if (size > 0)
		{
			EmptyClipboard();

			// Allocate a global memory object for the text. 
			HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (size + 1) * sizeof(char));
			if (hglbCopy) 
			{ 
				char * lptstrCopy = (char *)GlobalLock(hglbCopy); 
				memcpy(lptstrCopy, buff, size);
				lptstrCopy[size] = 0;
				GlobalUnlock(hglbCopy);
				SetClipboardData(CF_TEXT, hglbCopy);
			}
		}

		delete[] buff;
		CloseClipboard();
	}
}

// paste key setting
void config_paste_key_setting()
{
	if (OpenClipboard(NULL))
	{
		if (IsClipboardFormatAvailable(CF_TEXT))
		{
			HGLOBAL hglb = GetClipboardData(CF_TEXT); 
			if (hglb != NULL) 
			{ 
				char * lptstr = (char *)GlobalLock(hglb); 

				if (lptstr != NULL) 
				{
					config_parse_keymap(lptstr);
					GlobalUnlock(hglb); 
				}
			}
		}
		CloseClipboard();
	}
}


// get keyboard bind
void config_get_key_bind(byte code, key_bind_t * keydown, key_bind_t * keyup)
{
	if (keydown) *keydown = settings[current_setting].key_map[code][0];
	if (keyup) *keyup = settings[current_setting].key_map[code][1];
}

// set keyboard action
void config_set_key_bind(byte code, key_bind_t * keydown, key_bind_t * keyup)
{
	if (keydown) settings[current_setting].key_map[code][0] = *keydown;
	if (keyup) settings[current_setting].key_map[code][1] = *keyup;
}

// set keyboard label
void config_set_key_label(byte code, const char * label)
{
	strncpy(settings[current_setting].key_label[code].text, label ? label : "", sizeof(key_label_t));
}

// set keyboard label 
const char* config_get_key_label(byte code)
{
	return settings[current_setting].key_label[code].text;
}

// set key signature
void config_set_key_signature(char key)
{
	settings[current_setting].key_signature = key;
}

// get key signature
char config_get_key_signature()
{
	return settings[current_setting].key_signature;
}

// set oct shift
void config_set_key_octshift(byte channel, char shift)
{
	if (channel < ARRAY_COUNT(settings[current_setting].key_octshift))
	{
		settings[current_setting].key_octshift[channel] = shift;
	}
}

// get oct shift
char config_get_key_octshift(byte channel)
{
	if (channel < ARRAY_COUNT(settings[current_setting].key_octshift))
		return settings[current_setting].key_octshift[channel];
	else
		return 0;
}

// set velocity
void config_set_key_velocity(byte channel, byte velocity)
{
	if (channel < ARRAY_COUNT(settings[current_setting].key_velocity))
	{
		settings[current_setting].key_velocity[channel] = velocity;
	}
}

// get velocity
byte config_get_key_velocity(byte channel)
{
	if (channel < ARRAY_COUNT(settings[current_setting].key_velocity))
		return settings[current_setting].key_velocity[channel];
	else
		return 0;
}

// get channel
int config_get_key_channel(byte channel)
{
	if (channel < ARRAY_COUNT(settings[current_setting].key_channel))
		return settings[current_setting].key_channel[channel];
	else
		return 0;
}

// get channel
void config_set_key_channel(byte channel, byte value)
{
	if (channel < ARRAY_COUNT(settings[current_setting].key_channel))
	{
		settings[current_setting].key_channel[channel] = value;
	}
}

// get midi program
byte config_get_program(byte channel)
{
	if (channel < ARRAY_COUNT(settings[current_setting].midi_program))
		return settings[current_setting].midi_program[channel];
	else
		return 0;
}

// set midi program
void config_set_program(byte channel, byte value)
{
	if (channel < ARRAY_COUNT(settings[current_setting].midi_program))
	{
		settings[current_setting].midi_program[channel] = value;
	}
}

// get midi program
byte config_get_controller(byte channel, byte id)
{
	if (channel < ARRAY_COUNT(settings[current_setting].midi_controller))
		return settings[current_setting].midi_controller[channel][id];
	else
		return 0;
}

// set midi program
void config_set_controller(byte channel, byte id, byte value)
{
	if (channel < ARRAY_COUNT(settings[current_setting].midi_controller))
	{
		settings[current_setting].midi_controller[channel][id] = value;
	}
}

// get auto pedal time
char config_get_auto_pedal()
{
	return settings[current_setting].auto_pedal;
}

// set auto pedal time
void config_set_auto_pedal(char value)
{
	if (value < 0)
		value = 0;

	settings[current_setting].auto_pedal = value;
}

// reset config
static void config_reset()
{
	// reset global settings
	memset(&global, 0, sizeof(global));
	global.output_delay = 10;
	global.output_volume = 100;

	// keep only one setting group
	config_set_setting_group_count(1);

	// clear current setting group
	config_clear_key_setting();
}

// get setting group
uint config_get_setting_group()
{
	return current_setting;
}

// set setting group
void config_set_setting_group(uint id)
{
	if (id < setting_count)
		current_setting = id;

	// update status
	for (int ch = 0; ch < 16; ch++)
	{
		for (int i = 0; i < 127; i++)
			if (config_get_controller(ch, i) < 128)
				midi_send_event(0xb0 | ch, i, config_get_controller(ch, i), 0);

		if (config_get_program(ch) < 128)
			midi_send_event(0xc0 | ch, config_get_program(ch), 0, 0);
	}

}

// get setting group count
uint config_get_setting_group_count()
{
	return setting_count;
}

// set setting group count
void config_set_setting_group_count(uint count)
{
	if (count < 1)
		count = 1;

	if (count > ARRAY_COUNT(settings))
		count = ARRAY_COUNT(settings);

	// new setting id
	uint setting_id = current_setting < count ? current_setting : count - 1;

	// clear newly added settings.
	for (current_setting = setting_count; current_setting < count; current_setting ++)
		config_clear_key_setting();

	current_setting = setting_id;
	setting_count = count;
}

// set keyup mode
void config_set_delay_keyup(byte channel, char value)
{
	if (channel < ARRAY_COUNT(settings[current_setting].delay_keyup))
		settings[current_setting].delay_keyup[channel] = value;
}

// get keyup mode 
char config_get_delay_keyup(byte channel)
{
	if (channel < ARRAY_COUNT(settings[current_setting].delay_keyup))
		return settings[current_setting].delay_keyup[channel];
	else
		return 0;
}

// -----------------------------------------------------------------------------------------
// configuration save and load
// -----------------------------------------------------------------------------------------
static bool match_space(char ** str)
{
	char * s = *str;
	while (*s == ' ' || *s == '\t') s++;

	if (s == *str)
		return false;

	*str = s;
	return true;
}

static bool match_end(char ** str)
{
	bool result = false;
	char * s = *str;
	while (*s == ' ' || *s == '\t') s++;
	if (*s == '\r' || *s == '\n' || *s == '\0') result = true;
	while (*s == '\r' || *s == '\n') s++;

	if (result)
		*str = s;
	return result;
}

static bool match_word(char ** str, const char * match)
{
	char * s = *str;

	while (*match)
	{
		if (tolower(*s) != tolower(*match))
			return false;

		match++;
		s++;
	}

	if (match_space(&s) || match_end(&s))
	{
		*str = s;
		return true;
	}

	return false;
}

// match number
static bool match_number(char ** str, uint * value)
{
	uint v = 0;
	char * s = *str;

	if (*s == '-' || *s == '+')
		s++;

	if (*s >= '0' && *s <= '9')
	{
		while (*s >= '0' && *s <= '9')
			v = v * 10 + (*s++ - '0');

		if (!match_space(&s) && !match_end(&s))
			return false;

		*value = v;
		*str = s;
		return true;
	}

	return false;
}

static bool match_number(char ** str, int * value)
{
	int v = 0;
	char * s = *str;

	if (*s == '-' || *s == '+')
		s++;

	if (*s >= '0' && *s <= '9')
	{
		while (*s >= '0' && *s <= '9')
			v = v * 10 + (*s++ - '0');

		if (!match_space(&s) && !match_end(&s))
			return false;

		*value = (**str == '-') ? -v : v;
		*str = s;
		return true;
	}

	return false;
}

// match line
static bool match_line(char ** str, char * buff, uint size)
{
	char * s = *str;
	while (*s == ' ' || *s == '\t') s++;

	char * e = s;
	while (*e != '\0' && *e != '\r' && *e != '\n') e++;

	char * f = e;
	while (f > s && (f[-1] == ' ' || f[-1] == '\t')) f--;

	if (f > s && f - s < (int)size)
	{
		memcpy(buff, s, f - s);
		buff[f - s] = 0;

		while (*e == '\r' || *e == '\n') e++;
		*str = e;
		return true;
	}

	return false;
}

// match config value
static bool match_value(char ** str, name_t * names, uint count, uint * value)
{
	for (name_t * n = names; n < names + count; n++)
	{
		if (match_word(str, n->name))
		{
			*value = n->value;
			return true;
		}
	}

	return match_number(str, value);
}

// match action
static bool match_event(char ** str, key_bind_t * e)
{
	uint action = 0;
	uint channel = 0;
	uint arg1 = 0;
	uint arg2 = 0;
	uint arg3 = 0;

	// match action
	if (!match_value(str, action_names, ARRAY_COUNT(action_names), &action))
		return false;

	if (action == 0)
		return false;

	// make action
	e->a = action;
	e->b = 0;
	e->c = 0;
	e->d = 0;

	if (action < 0x80)
	{
		switch (action)
		{
		case SM_SETTING_GROUP_COUNT:
			if (!match_value(str, 0, NULL, &arg1))
				return false;
			break;

		case SM_KEY_SIGNATURE:
		case SM_VOLUME:
		case SM_SETTING_GROUP:
		case SM_AUTO_PEDAL:
			if (!match_value(str, value_action_names, ARRAY_COUNT(value_action_names), &arg1))
				return false;

			if (!match_value(str, 0, NULL, &arg2))
				return false;

			break;

		case SM_OCTSHIFT:
		case SM_VELOCITY:
		case SM_CHANNEL:
		case SM_DELAY_KEYUP:
			if (!match_value(str, 0, NULL, &arg1))
				return false;

			if (!match_value(str, value_action_names, ARRAY_COUNT(value_action_names), &arg2))
				return false;

			if (!match_value(str, 0, NULL, &arg3))
				return false;

			break;

		default:
			match_value(str, NULL, 0, &arg1);
			match_value(str, NULL, 0, &arg2);
			match_value(str, NULL, 0, &arg3);
			break;
		}

		e->b = arg1;
		e->c = arg2;
		e->d = arg3;
	}
	
	// match midi events
	else
	{
		// match channel
		if (!match_number(str, &channel))
			return false;

		// make action
		e->a |= (channel & 0xf);
		e->b = 0;
		e->c = 0;
		e->d = 0;

		// parse args based on action
		switch (action >> 4)
		{
		case 0x0:
			break;

		case 0x8:	// NoteOff
		case 0x9:	// NoteOn
		case 0xa:	// NoteAfterTouch
			if (!match_value(str, note_names, ARRAY_COUNT(note_names), &arg1))
				return false;

			e->b = arg1 & 0x7f;
			e->c = match_number(str, &arg2) ? arg2 : 127;
			break;

		case 0xb:	// Controller
			if (!match_value(str, controller_names, ARRAY_COUNT(controller_names), &arg1))
				return false;

			if (!match_number(str, &arg2))
				return false;

			match_value(str, value_action_names, ARRAY_COUNT(value_action_names), &arg3);

			e->b = arg1 & 0x7f;
			e->c = arg2 & 0x7f;
			e->d = arg3;
			break;

		case 0xc:	// ProgramChange
			if (!match_number(str, &arg1))
				return false;

			match_value(str, value_action_names, ARRAY_COUNT(value_action_names), &arg2);

			e->b = arg1 & 0x7f;
			e->c = arg2;
			break;

		case 0xd:
			if (!match_number(str, &arg1))
				return false;

			e->b = arg1 & 0x7f;
			break;

		case 0xe:
			if (!match_number(str, &arg1))
				return false;

			e->b = arg1;
			e->c = arg1 >> 8;
			break;

		default:
			return false;
		}
	}

	return true;
}

static int config_parse_keymap_line(char * s)
{
	// skip comment
	if (*s == '#')
		return 0;

	// key 
	if (match_word(&s, "Key") || match_word(&s, "Keydown"))
	{
		uint key = 0;
		key_bind_t keydown;
		key_bind_t keyup;

		// match key name
		if (!match_value(&s, key_names, ARRAY_COUNT(key_names), &key))
			return -1;

		// match midi event
		if (match_event(&s, &keydown))
		{
			// set that action
			config_set_key_bind(key, &keydown, &keyup);
			return 0;
		}
	}

	// keyup
	else if (match_word(&s, "Keyup"))
	{
		uint key = 0;
		key_bind_t keyup;

		// match key name
		if (!match_value(&s, key_names, ARRAY_COUNT(key_names), &key))
			return -1;

		// match midi event
		if (match_event(&s, &keyup))
		{
			// set that action
			config_set_key_bind(key, NULL, &keyup);

			return 0;
		}
	}

	// keylabel
	else if (match_word(&s, "Label"))
	{
		uint key = 0;
		char buff[256];

		// match key name
		if (!match_value(&s, key_names, ARRAY_COUNT(key_names), &key))
			return -1;

		// text
		match_line(&s, buff, sizeof(buff));

		// set key label
		config_set_key_label(key, buff);
	}

	// octshift
	else if (match_word(&s, "Octshift"))
	{
		uint channel;
		int value;

		if (match_number(&s, &channel) &&
			match_number(&s, &value))
		{
			config_set_key_octshift(channel, value);
		}
	}

	// velocity
	else if (match_word(&s, "Velocity"))
	{
		uint channel;
		uint value;

		if (match_number(&s, &channel) &&
			match_number(&s, &value))
		{
			config_set_key_velocity(channel, value);
		}
	}

	// output channel
	else if (match_word(&s, "Channel"))
	{
		uint channel;
		uint value;

		if (match_number(&s, &channel) &&
			match_number(&s, &value))
		{
			config_set_key_channel(channel, value);
		}
	}

	else if (match_word(&s, "KeySignature"))
	{
		int value;

		if (match_number(&s, &value))
		{
			config_set_key_signature(value);
		}
	}

	else if (match_word(&s, "GroupCount"))
	{
		int value;

		if (match_number(&s, &value))
		{
			config_set_setting_group_count(value);

			for (int i = value; i > 0; i--)
			{
				config_set_setting_group(i - 1);
				config_clear_key_setting();
			}
		}
	}

	else if (match_word(&s, "Group"))
	{
		int value;

		if (match_number(&s, &value))
		{
			config_set_setting_group(value);
		}
	}

	else if (match_word(&s, "AutoPedal"))
	{
		int value;

		if (match_number(&s, &value))
		{
			config_set_auto_pedal(value);
		}
	}

	else if (match_word(&s, "DelayKeyup"))
	{
		uint channel;
		uint value;

		if (match_number(&s, &channel) &&
			match_number(&s, &value))
		{
			config_set_delay_keyup(channel, value);
		}
	}

	else if (match_word(&s, "Program"))
	{
		uint channel;
		uint value;

		if (match_number(&s, &channel) &&
			match_number(&s, &value))
		{
			config_set_program(channel, value);
		}
	}

	else if (match_word(&s, "Controller"))
	{
		uint channel;
		uint id;
		uint value;

		if (match_number(&s, &channel) &&
			match_number(&s, &id) &&
			match_number(&s, &value))
		{
			config_set_controller(channel, id, value);
		}
	}

	return -1;
}

int config_parse_keymap(const char * command)
{
	char line[1024];
	char * line_end = line;
	int result = 0;

	for (;;)
	{
		switch (*command)
		{
		case 0:
		case '\n':
			if (line_end > line)
			{
				*line_end = 0;
				result = config_parse_keymap_line(line);
				line_end = line;
			}

			if (*command == 0)
				return result;
			break;

		default:
			if (line_end < line + sizeof(line) - 1)
				*line_end++ = *command;
			break;
		}

		command++;
	}
}


// load keymap
int config_load_keymap(const char * filename)
{
	char line[256];
	config_get_media_path(line, sizeof(line), filename);

	FILE * fp = fopen(line, "r");
	if (!fp)
		return -1;

	// current group
	int group_id = config_get_setting_group();

	// clear keyboard binds
	config_clear_key_setting();

	// read lines
	while (fgets(line, sizeof(line), fp))
	{
		config_parse_keymap_line(line);
	}

	fclose(fp);

	// restore current group
	config_set_setting_group(group_id);
	return 0;
}

static int print_value(char * buff, int buff_size, int value, name_t * names, int name_count, const char * sep = "\t")
{
	for (int i = 0; i < name_count; i++)
	{
		if (names[i].value == value)
		{
			return _snprintf(buff, buff_size, "%s%s", sep, names[i].name);
		}
	}

	return _snprintf(buff, buff_size, "%s%d", sep, value);
}

static int print_keyboard_event(char * buff, int buffer_size, int key, key_bind_t & e)
{
	char * s = buff;
	char * end = buff + buffer_size;

	s += print_value(s, end - s, key, key_names, ARRAY_COUNT(key_names));

	if (e.a < 0x80)
	{
		s += print_value(s, end - s, e.a, action_names, ARRAY_COUNT(action_names));

		switch (e.a)
		{
		case SM_KEY_SIGNATURE:	// key
		case SM_VOLUME:	// volume
			s += print_value(s, end - s, e.b, value_action_names, ARRAY_COUNT(value_action_names));
			s += print_value(s, end - s, e.c, NULL, 0);
			break;

		case SM_OCTSHIFT:	// oct
		case SM_VELOCITY:	// vel
		case SM_CHANNEL:	// channel
		case SM_DELAY_KEYUP:
			s += print_value(s, end - s, e.b, NULL, 0);
			s += print_value(s, end - s, e.c, value_action_names, ARRAY_COUNT(value_action_names));
			s += print_value(s, end - s, e.d, NULL, 0);
			break;

		case SM_PLAY:	// play
		case SM_RECORD:	// rec
		case SM_STOP:	// stop
			break;

		default:
			s += print_value(s, end - s, e.b, NULL, 0);
			s += print_value(s, end - s, e.c, NULL, 0);
			s += print_value(s, end - s, e.d, NULL, 0);
			break;
		}
	}
	else
	{
		s += print_value(s, end - s, e.a & 0xf0, action_names, ARRAY_COUNT(action_names));
		s += print_value(s, end - s, e.a & 0x0f, NULL, 0);

		switch (e.a >> 4)
		{
		case 0x8:
		case 0x9:
			s += print_value(s, end - s, e.b, note_names, ARRAY_COUNT(note_names));
			if (e.c != 127)
				s += print_value(s, end - s, e.c, NULL, 0);
			break;

		case 0xa:
		case 0xd:
			s += print_value(s, end - s, e.b, note_names, ARRAY_COUNT(note_names));
			s += print_value(s, end - s, e.c, NULL, 0);
			break;

		case 0xb:
			s += print_value(s, end - s, e.b, controller_names, ARRAY_COUNT(controller_names));
			s += print_value(s, end - s, e.c, NULL, 0);

			if (e.d) s += print_value(s, end - s, e.d, value_action_names, ARRAY_COUNT(controller_names));
			break;

		case 0xc:
			s += print_value(s, end - s, e.b, NULL, 0);
			if (e.c) s += print_value(s, end - s, e.c, value_action_names, ARRAY_COUNT(controller_names));
			break;

		default:
			s += print_value(s, end - s, e.b, NULL, 0);
			s += print_value(s, end - s, e.c, NULL, 0);
			s += print_value(s, end - s, e.d, NULL, 0);
			break;
		}
	}


	return s - buff;
}

// save current key settings
static int config_save_key_settings(char * buff, int buffer_size)
{
	char * end = buff + buffer_size;
	char * s = buff;

	// save key signature
	s += _snprintf(s, end - s, "KeySignature\t%d\r\n", config_get_key_signature());

	// save keyboard status
	for (int ch = 0; ch < 16; ch ++)
	{
		if (config_get_key_octshift(ch) != 0)
			s += _snprintf(s, end - s, "Octshift\t%d\t%d\r\n", ch, config_get_key_octshift(ch));

		if (config_get_key_velocity(ch) != 127)
			s += _snprintf(s, end - s, "Velocity\t%d\t%d\r\n", ch, config_get_key_velocity(ch));

		if (config_get_key_channel(ch) != 0)
			s += _snprintf(s, end - s, "Channel\t%d\t%d\r\n", ch, config_get_key_channel(ch));

		// program
		if (config_get_program(ch) < 128)
			s += _snprintf(s, end - s, "Program\t%d\t%d\r\n", ch, config_get_program(ch));

		// controller
		for (int id = 0; id < 127; id++)
		{
			if (config_get_controller(ch, id) < 128)
			{
				s += _snprintf(s, end - s, "Controller\t%d", ch);
				s += print_value(s, end - s, id, controller_names, ARRAY_COUNT(controller_names));
				s += print_value(s, end - s, config_get_controller(ch, id), NULL, NULL);
				s += _snprintf(s, end - s, "\r\n");
			}
		}

		// delay keyup
		if (config_get_delay_keyup(ch))
			s += _snprintf(s, end - s, "DelayKeyup\t%d\t%d\r\n", ch, config_get_delay_keyup(ch));

	}

	// auto pedal
	if (config_get_auto_pedal())
		s += _snprintf(s, end - s, "AutoPedal\t%d\r\n", config_get_auto_pedal());

	// save key bindings
	for (int key = 0; key < 256; key++)
	{
		key_bind_t keydown, keyup;
		config_get_key_bind(key, &keydown, &keyup);

		if (keydown.a)
		{
			s += _snprintf(s, end - s, "Keydown");
			s += print_keyboard_event(s, end - s, key, keydown);
			s += _snprintf(s, end - s, "\r\n");
		}

		if (keyup.a)
		{
			s += _snprintf(s, end - s, "Keyup");
			s += print_keyboard_event(s, end - s, key, keyup);
			s += _snprintf(s, end - s, "\r\n");
		}
	}

	for (int key = 0; key < 256; key++)
	{
		if (*config_get_key_label(key))
		{
			s += _snprintf(s, end - s, "Label");
			s += print_value(s, end - s, key, key_names, ARRAY_COUNT(key_names));
			s += _snprintf(s, end - s, "\t%s\r\n", config_get_key_label(key));
		}
	}

	return s - buff;
}

// save keymap
int config_save_keymap(char * buff, int buffer_size)
{
	char * end = buff + buffer_size;
	char * s = buff;

	// save group count
	s += _snprintf(s, end - s, "GroupCount\t%d\r\n", config_get_setting_group_count());

	// current setting group
	int current_setting = config_get_setting_group();

	// for each group
	for (uint i = 0; i < config_get_setting_group_count(); i++)
	{
		// change group
		s += _snprintf(s, end - s, "\r\nGroup\t%d\r\n", i);

		// change to group i
		config_set_setting_group(i);

		// save current key settings
		s += config_save_key_settings(s, end - s);
	}

	// restore current setting group
	config_set_setting_group(current_setting);

	return s - buff;
}

static int config_apply()
{
	// set keymap
	config_set_keymap(global.keymap);

	// load instrument
	config_select_instrument(global.instrument_type, global.instrument_path);

	// open output
	if (config_select_output(global.output_type, global.output_device))
		global.output_device[0] = 0;

	config_set_output_volume(global.output_volume);
	config_set_output_delay(global.output_delay);

	// open midi output and input
	config_select_midi_output(global.midi_output);
	config_select_midi_input(global.midi_input);

	// reset default key setting
	if (global.keymap[0] == 0)
		config_default_key_setting();
	return 0;
}

// load
int config_load(const char * filename)
{
	char line[256];
	config_get_media_path(line, sizeof(line), filename);

	// reset config
	config_reset();

	FILE * fp = fopen(line, "r");
	if (fp)
	{
		// read lines
		while (fgets(line, sizeof(line), fp))
		{
			char * s = line;

			// comment
			if (*s == '#')
				continue;

			// instrument
			if (match_word(&s, "instrument"))
			{
				// instrument type
				if (match_word(&s, "type"))
				{
					match_value(&s, instrument_type_names, ARRAY_COUNT(instrument_type_names), &global.instrument_type);
				}
				else if (match_word(&s, "path"))
				{
					match_line(&s, global.instrument_path, sizeof(global.instrument_path));
				}

				else if (match_word(&s, "showui"))
				{
					uint showui = 1;
					match_value(&s, NULL, 0, &showui);
					vsti_show_editor(showui != 0);
				}
			}

			// output
			else if (match_word(&s, "output"))
			{
				// instrument type
				if (match_word(&s, "type"))
				{
					match_value(&s, output_type_names, ARRAY_COUNT(output_type_names), &global.output_type);
				}
				else if (match_word(&s, "device"))
				{
					match_line(&s, global.output_device, sizeof(global.output_device));
				}
				else if (match_word(&s, "delay"))
				{
					match_number(&s, &global.output_delay);
				}
				else if (match_word(&s, "volume"))
				{
					match_number(&s, &global.output_volume);
				}
			}

			// keyboard
			else if (match_word(&s, "keyboard"))
			{
				if (match_word(&s, "map"))
				{
					match_line(&s, global.keymap, sizeof(global.keymap));
				}
			}

			// midi
			else if (match_word(&s, "midi"))
			{
				if (match_word(&s, "output"))
				{
					match_line(&s, global.midi_output, sizeof(global.midi_output));
				}

				else if (match_word(&s, "input"))
				{
					match_line(&s, global.midi_input, sizeof(global.midi_input));
				}
			}

			// resize window
			else if (match_word(&s, "sizeable"))
			{
				uint enable = 0;
				match_value(&s, boolean_names, ARRAY_COUNT(boolean_names), &enable);
				config_set_enable_resize_window(enable != 0);
			}

			// hotkey
			else if (match_word(&s, "hotkey"))
			{
				uint enable = 0;
				match_value(&s, boolean_names, ARRAY_COUNT(boolean_names), &enable);
				config_set_enable_hotkey(enable != 0);
			}
		}

		fclose(fp);
	}

	return config_apply();
}

// save
int config_save(const char * filename)
{
	char file_path[256];
	config_get_media_path(file_path, sizeof(file_path), filename);

	FILE * fp = fopen(file_path, "wb");
	if (!fp)
		return -1;

	if (global.instrument_type)
	{
		fprintf(fp, "instrument type %s\r\n", instrument_type_names[global.instrument_type]);
		if (global.instrument_path[0])
			fprintf(fp, "instrument path %s\r\n", global.instrument_path);

		fprintf(fp, "instrument showui %d\r\n", vsti_is_show_editor());
	}


	if (global.output_type)
	{
		fprintf(fp, "output type %s\r\n", output_type_names[global.output_type]);

		if (global.output_device[0])
			fprintf(fp, "output device %s\r\n", global.output_device);

		fprintf(fp, "output delay %d\r\n", global.output_delay);
		fprintf(fp, "output volume %d\r\n", global.output_volume);
	}

	if (global.keymap[0])
		fprintf(fp, "keyboard map %s\r\n", global.keymap);

	if (global.midi_output[0])
		fprintf(fp, "midi output %s\r\n", global.midi_output);

	if (global.midi_input[0])
		fprintf(fp, "midi input %s\n", global.midi_input);

	if (config_get_enable_hotkey())
		fprintf(fp, "hotkey enable\n");

	if (config_get_enable_resize_window())
		fprintf(fp, "resize enable\n");

	fclose(fp);
	return 0;
}

// initialize config
int config_init()
{
	config_reset();
	return 0;
}

// close config
void config_shutdown()
{
	midi_close_input();
	midi_close_output();
	vsti_unload_plugin();
	asio_close();
	dsound_close();
	wasapi_close();
}


// select instrument
int config_select_instrument(int type, const char * name)
{
	int result = -1;

	if (type == INSTRUMENT_TYPE_NONE)
	{
		vsti_unload_plugin();
		global.instrument_type = type;
		strncpy(global.instrument_path, name, sizeof(global.instrument_path));
		result = 0;
	}

	else if (type == INSTRUMENT_TYPE_VSTI)
	{
		if (name == NULL || name[0] == '\0')
		{
			vsti_unload_plugin();
			global.instrument_path[0] = 0;
			result = 0;
		}

		else if (PathIsFileSpec(name))
		{
			struct select_instrument_cb : vsti_enum_callback
			{
				void operator ()(const char * value)
				{
					if (!found)
					{
						if (_stricmp(PathFindFileName(value), name) == 0)
						{
							strncpy(name, value, sizeof(name));
							found = true;
						}
					}
				}

				char name[256];
				bool found;
			};

			select_instrument_cb callback;
			_snprintf(callback.name, sizeof(callback.name), "%s.dll", name);
			callback.found = false;

			vsti_enum_plugins(callback);

			if (callback.found)
			{
				vsti_unload_plugin();

				// load plugin
				result = vsti_load_plugin(callback.name);
				if (result == 0)
				{
					global.instrument_type = type;
					strncpy(global.instrument_path, name, sizeof(global.instrument_path));
				}
			}
		}
		else
		{
			vsti_unload_plugin();

			// load plugin
			result = vsti_load_plugin(name);
			if (result == 0)
			{
				global.instrument_type = type;
				strncpy(global.instrument_path, name, sizeof(global.instrument_path));
			}
		}
	}

	midi_reset();

	return result;
}

const char * config_get_instrument_path()
{
	return global.instrument_path;
}

// set enable resize window
void config_set_enable_resize_window(bool enable)
{
	global.enable_resize = enable;
}

// get enable resize windwo
bool config_get_enable_resize_window()
{
	return global.enable_resize != 0;
}

// set enable resize window
void config_set_enable_hotkey(bool enable)
{
	global.enable_hotkey = enable;
}

// get enable resize windwo
bool config_get_enable_hotkey()
{
	return global.enable_hotkey != 0;
}


// select output
int config_select_output(int type, const char * device)
{
	int result = -1;
	asio_close();
	dsound_close();
	wasapi_close();

	// open output
	switch (type)
	{
	case OUTPUT_TYPE_AUTO:
		// try wasapi first
		result = wasapi_open(NULL);

		if (result)
			result = dsound_open(device);

		if (result == 0)
			device = "";

		break;

	case OUTPUT_TYPE_DSOUND:
		result = dsound_open(device);
		break;

	case OUTPUT_TYPE_WASAPI:
		result = wasapi_open(device);
		break;

	case OUTPUT_TYPE_ASIO:
		result = asio_open(device);
		break;
	}

	// success
	if (result == 0)
	{
		global.output_type = type;
		strncpy(global.output_device, device, sizeof(global.output_device));
	}

	return result;
}


// select midi input
int config_select_midi_input(const char * device)
{
	int result = -1;
	if (result = midi_open_input(device))
	{
		global.midi_input[0] = 0;
		return result;
	}

	strncpy(global.midi_input, device, sizeof(global.midi_input));
	return result;
}

// get midi input
const char * config_get_midi_input()
{
	return global.midi_input;
}

// select midi output
int config_select_midi_output(const char * device)
{
	int result = -1;
	if (result = midi_open_output(device))
	{
		return result;
	}

	strncpy(global.midi_output, device, sizeof(global.midi_output));
	return result;
}

// get midi output
const char * config_get_midi_output()
{
	return global.midi_output;
}

// get output delay
int config_get_output_delay()
{
	return global.output_delay;
}

// get output delay
void config_set_output_delay(int delay)
{
	if (delay < 0)
		delay = 0;
	else if (delay > 500)
		delay = 500;

	global.output_delay = delay;

	// open output
	switch (global.output_type)
	{
	case OUTPUT_TYPE_AUTO:
		dsound_set_buffer_time(global.output_delay);
		wasapi_set_buffer_time(global.output_delay);
		break;

	case OUTPUT_TYPE_DSOUND:
		dsound_set_buffer_time(global.output_delay);
		break;

	case OUTPUT_TYPE_WASAPI:
		wasapi_set_buffer_time(global.output_delay);
		break;

	case OUTPUT_TYPE_ASIO:
		break;
	}
}

// get output type
int config_get_output_type()
{
	return global.output_type;
}

// get output device
const char * config_get_output_device()
{
	return global.output_device;
}

// get keymap
const char * config_get_keymap()
{
	return global.keymap;
}

// set keymap
int config_set_keymap(const char * mapname)
{
	char path[MAX_PATH];
	config_get_media_path(path, sizeof(path), mapname);

	int result;
	if (result = config_load_keymap(mapname))
	{
		global.keymap[0] = 0;
	}
	else
	{
		config_get_relative_path(global.keymap, sizeof(global.keymap), path);
	}
	return result;
}


// get exe path
void config_get_media_path(char * buff, int buff_size, const char * path)
{
	if (PathIsRelative(path))
	{
		char base[MAX_PATH];
		char temp[MAX_PATH];

		GetModuleFileNameA(NULL, base, sizeof(base));

		// remove file spec
		PathRemoveFileSpec(base);

		// to media path
#ifdef _DEBUG
		PathAppend(base, "\\..\\");
#else
		PathAppend(base, "\\.\\");
#endif

		PathCombine(temp, base, path);
		strncpy(buff, temp, buff_size);
	}
	else
	{
		strncpy(buff, path, buff_size);
	}

}

// get relative path
void config_get_relative_path(char * buff, int buff_size, const char * path)
{
	if (PathIsRelative(path))
	{
		strncpy(buff, path, buff_size);
	}
	else
	{
		char base[MAX_PATH];
		config_get_media_path(base, sizeof(base), "");

		if (_strnicmp(path, base, strlen(base)) == 0)
		{
			strncpy(buff, path + strlen(base), buff_size);
		}
		else
		{
			strncpy(buff, path, buff_size);
		}
	}
}


// get volume
int config_get_output_volume()
{
	return global.output_volume;
}

// set volume
void config_set_output_volume(int volume)
{
	if (volume < 0) volume = 0;
	if (volume > 100) volume = 100;
	global.output_volume = volume;
}

// reset settings
void config_default_key_setting()
{
	config_clear_key_setting();

	// find resource
	HRSRC hrsrc = FindResource(0, MAKEINTRESOURCE(IDR_TEXT_DEFAULT_SETTING), "TEXT");

	if (hrsrc)
	{
		// load resource
		HGLOBAL hrc = LoadResource(0, hrsrc);

		if (hrc)
		{
			char line[1024];
			char * line_end = line;

			char* data = (char*)LockResource(hrc);
			char* end = data + SizeofResource(NULL, hrsrc);

			while (data < end)
			{
				if (*data == '\n')
				{
					if (line_end > line)
					{
						*line_end = 0;
						config_parse_keymap_line(line);
						line_end = line;
					}
				}
				else
				{
					if (line_end < line + sizeof(line) - 1)
						*line_end++ = *data;
				}

				data++;
			}
			
			// last line
			if (line_end > line)
			{
				*line_end = 0;
				config_parse_keymap_line(line);
				line_end = line;
			}

			FreeResource(hrc);
		}
	}
}

// get key name
const char * config_get_key_name(byte code)
{
	for (name_t * name = key_names; name < key_names + ARRAY_COUNT(key_names); name++)
		if (name->value == code)
			return name->name;

	static char buff[16];
	_snprintf(buff, sizeof(buff), "%d", code);
	return buff;
}