#include "pch.h"
#include <dinput.h>

#include "stdio.h"
#include "keyboard.h"
#include "output_asio.h"
#include "output_dsound.h"
#include "output_wasapi.h"
#include "synthesizer_vst.h"

// -----------------------------------------------------------------------------------------
// config parse functions
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
	{ "NoteOn",				0x9 },
	{ "NoteOff",			0x8 },
	{ "NoteAftertouch",		0xa },
	{ "Controller",			0xb },
	{ "ProgramChange",		0xc },
	{ "ChannelAftertouch",	0xd },
	{ "PitchBend",			0xe },
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
static bool match_midi_event(char ** str, MidiEvent * e)
{
	uint action = 0;
	uint channel = 0;
	uint arg1 = 0;
	uint arg2 = 0;

	// match action
	if (!match_value(str, action_names, ARRAY_COUNT(action_names), &action))
		return false;

	// match action
	if (!match_number(str, &channel))
		return false;

	// make action
	e->action = (action << 4) | (channel & 0xF);
	e->arg1 = 0;
	e->arg2 = 0;
	e->arg3 = 0;

	// parse args based on action
	switch (action)
	{
	case 0x0:
		break;

	case 0x8: case 0x9: case 0xa:
		if (!match_value(str, note_names, ARRAY_COUNT(note_names), &arg1))
			return false;

		e->arg1 = arg1 & 0x7f;
		e->arg2 = match_number(str, &arg2) ? arg2 : 127;
		break;

	case 0xb:
		if (!match_value(str, controller_names, ARRAY_COUNT(controller_names), &arg1))
			return false;

		if (!match_number(str, &arg2))
			return false;

		e->arg1 = arg1 & 0x7f;
		e->arg2 = arg2 & 0x7f;
		break;

	case 0xc: case 0xd:
		if (!match_number(str, &arg1))
			return false;

		e->arg1 = arg1 & 0x7f;
		break;

	case 0xe:
		if (!match_number(str, &arg1))
			return false;

		e->arg1 = arg1;
		e->arg2 = arg1 >> 8;
		break;

	default:
		return false;
	}

	return true;
}

static bool default_midi_keyup_event(MidiEvent * keydown, MidiEvent * keyup)
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
};

// -----------------------------------------------------------------------------------------
// configuration
// -----------------------------------------------------------------------------------------
#define	INSTRUMENT_TYPE_UNKNOWN		0
#define	INSTRUMENT_TYPE_VSTI		1

#define	OUTPUT_TYPE_UNKNOWN			0
#define	OUTPUT_TYPE_MIDI			1
#define	OUTPUT_TYPE_DSOUND			2
#define	OUTPUT_TYPE_ASIO			3
#define	OUTPUT_TYPE_WASAPI			4

static name_t instrument_type_names[] = 
{
	{ "Unknown",		INSTRUMENT_TYPE_UNKNOWN },
	{ "VSTI",			INSTRUMENT_TYPE_VSTI },
};

static name_t output_type_names[] = 
{
	{ "Unknown",		OUTPUT_TYPE_UNKNOWN },
	{ "MIDI",			OUTPUT_TYPE_MIDI },
	{ "DirectSound",	OUTPUT_TYPE_DSOUND },
	{ "ASIO",			OUTPUT_TYPE_ASIO },
	{ "wasapi",			OUTPUT_TYPE_WASAPI },
};

static uint	cfg_instrument_type = 0;
static char cfg_instrument_path[256] = {0};
static uint	cfg_output_type = 0;
static char cfg_output_device[256] = {0};
static uint cfg_output_delay = 0;
static char cfg_keymap[256] = {0};

// load keymap
int config_load_keymap(const char * filename)
{
	char line[256];

	FILE * fp = fopen(filename, "r");
	if (!fp)
		return -1;

	// reset keyboard map
	for (int code = 0; code < 256; code++)
	{
		MidiEvent empty = {0, 0, 0, 0};
		keyboard_set_map(code, &empty, &empty);
	}

	// read lines
	while (fgets(line, sizeof(line), fp))
	{
		char * s = line;

		// comment
		if (*s == '#')
			continue;

		// key 
		if (match_word(&s, "key"))
		{
			uint key = 0;
			MidiEvent keydown;
			MidiEvent keyup;

			// match key name
			if (!match_value(&s, key_names, ARRAY_COUNT(key_names), &key))
				continue;

			// match midi event
			if (match_midi_event(&s, &keydown))
			{
				// generate default midi keyup event
				default_midi_keyup_event(&keydown, &keyup);

				// set that action
				keyboard_set_map(key, &keydown, &keyup);
			}
		}

		// keydown
		else if (match_word(&s, "keydown"))
		{
			uint key = 0;
			MidiEvent keydown;

			// match key name
			if (!match_value(&s, key_names, ARRAY_COUNT(key_names), &key))
				continue;

			// match midi event
			if (match_midi_event(&s, &keydown))
			{
				// set that action
				keyboard_set_map(key, &keydown, NULL);
			}
		}

		// keyup
		else if (match_word(&s, "keyup"))
		{
			uint key = 0;
			MidiEvent keyup;

			// match key name
			if (!match_value(&s, key_names, ARRAY_COUNT(key_names), &key))
				continue;

			// match midi event
			if (match_midi_event(&s, &keyup))
			{
				// set that action
				keyboard_set_map(key, NULL, &keyup);
			}
		}
	}

	fclose(fp);
	return 0;
}

// reset config
void config_reset()
{
	cfg_instrument_type = 0;
	cfg_instrument_path[0] = 0;
	cfg_output_type = 0;
	cfg_output_device[0] = 0;
	cfg_output_delay = 0;

	for (byte ch = 0; ch < 16; ch++)
	{
		keyboard_set_octshift(ch, 0);
		keyboard_set_velocity(ch, 127);
	}

	midi_set_octshift(0);
}

// apply config
static int config_apply()
{
	// set keymap
	if (cfg_keymap[0])
		config_load_keymap(cfg_keymap);

	// load instrument
	switch (cfg_instrument_type)
	{
	case INSTRUMENT_TYPE_VSTI:	vsti_load_plugin(cfg_instrument_path);	break;
	}

	// open output
	switch (cfg_output_type)
	{
	case OUTPUT_TYPE_MIDI:
		{
			midi_open_output(cfg_output_device);
		}
		break;

	case OUTPUT_TYPE_DSOUND:
		{
			dsound_open(cfg_output_device);
		}
		break;

	case OUTPUT_TYPE_ASIO:
		{
			asio_open(cfg_output_device);
		}
		break;

	case OUTPUT_TYPE_WASAPI:
		{
			wasapi_open(cfg_output_device);
			wasapi_set_buffer_time(cfg_output_delay);
		}
		break;
	}
	return 0;
}


// load
int config_load(const char * filename)
{
	char line[256];

	FILE * fp = fopen(filename, "r");
	if (!fp)
		return -1;

	// reset config
	config_reset();

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
				match_value(&s, instrument_type_names, ARRAY_COUNT(instrument_type_names), &cfg_instrument_type);
			}
			else if (match_word(&s, "path"))
			{
				match_line(&s, cfg_instrument_path, sizeof(cfg_instrument_path));
			}
		}

		// output
		else if (match_word(&s, "output"))
		{
			// instrument type
			if (match_word(&s, "type"))
			{
				match_value(&s, output_type_names, ARRAY_COUNT(output_type_names), &cfg_output_type);
			}
			else if (match_word(&s, "device"))
			{
				match_line(&s, cfg_output_device, sizeof(cfg_output_device));
			}
			else if (match_word(&s, "delay"))
			{
				match_number(&s, &cfg_output_delay);
			}
		}

		// keyboard
		else if (match_word(&s, "keyboard"))
		{
			if (match_word(&s, "shift"))
			{
				uint channel;
				int value;

				if (match_number(&s, &channel) &&
					match_number(&s, &value))
				{
					keyboard_set_octshift(channel, value);
				}
			}

			else if (match_word(&s, "velocity"))
			{
				uint channel;
				uint value;

				if (match_number(&s, &channel) &&
					match_number(&s, &value))
				{
					keyboard_set_velocity(channel, value);
				}
			}

			else if (match_word(&s, "map"))
			{
				match_line(&s, cfg_keymap, sizeof(cfg_keymap));
			}
		}

		// midi
		else if (match_word(&s, "midi"))
		{
			if (match_word(&s, "shift"))
			{
				int value;

				if (match_number(&s, &value))
				{
					midi_set_octshift(value);
				}
			}
		}
	}

	fclose(fp);

	return config_apply();
}

// save
int config_save(const char * filename)
{
	return 0;
}

// close config
void config_close()
{
	midi_close_input();
	midi_close_output();
	asio_close();
	dsound_close();
	wasapi_close();
}
