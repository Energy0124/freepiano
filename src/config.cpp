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
static bool match_midi_event(char ** str, KeyboardEvent * e)
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

// -----------------------------------------------------------------------------------------
// configuration
// -----------------------------------------------------------------------------------------
static name_t instrument_type_names[] = 
{
	{ "None",			INSTRUMENT_TYPE_NONE },
	{ "VSTI",			INSTRUMENT_TYPE_VSTI },
};

static name_t output_type_names[] = 
{
	{ "None",			OUTPUT_TYPE_NONE },
	{ "DirectSound",	OUTPUT_TYPE_DSOUND },
	{ "Wasapi",			OUTPUT_TYPE_WASAPI },
	{ "ASIO",			OUTPUT_TYPE_ASIO },
};

static uint	cfg_instrument_type = 0;
static char cfg_instrument_path[256] = {0};
static uint	cfg_output_type = 0;
static char cfg_output_device[256] = {0};
static uint cfg_output_delay = 10;
static char cfg_keymap[256] = {0};
static char cfg_midi_input[256] = {0};
static char cfg_midi_output[256] = {0};

int config_keymap_config(char * command)
{
	char * s = command;

	// key 
	if (match_word(&s, "key"))
	{
		uint key = 0;
		KeyboardEvent keydown;
		KeyboardEvent keyup;

		// match key name
		if (!match_value(&s, key_names, ARRAY_COUNT(key_names), &key))
			return -1;

		// match midi event
		if (match_midi_event(&s, &keydown))
		{
			// generate default midi keyup event
			keyboard_default_keyup(&keydown, &keyup);

			// set that action
			keyboard_set_map(key, &keydown, &keyup);

			return 0;
		}
	}

	// keydown
	else if (match_word(&s, "keydown"))
	{
		uint key = 0;
		KeyboardEvent keydown;

		// match key name
		if (!match_value(&s, key_names, ARRAY_COUNT(key_names), &key))
			return -1;

		// match midi event
		if (match_midi_event(&s, &keydown))
		{
			// set that action
			keyboard_set_map(key, &keydown, NULL);

			return 0;
		}
	}

	// keyup
	else if (match_word(&s, "keyup"))
	{
		uint key = 0;
		KeyboardEvent keyup;

		// match key name
		if (!match_value(&s, key_names, ARRAY_COUNT(key_names), &key))
			return -1;

		// match midi event
		if (match_midi_event(&s, &keyup))
		{
			// set that action
			keyboard_set_map(key, NULL, &keyup);

			return 0;
		}
	}

	return -1;
}

// load keymap
int config_load_keymap(const char * filename)
{
	char line[256];
	config_get_media_path(line, sizeof(line), filename);

	FILE * fp = fopen(line, "r");
	if (!fp)
		return -1;

	// reset keyboard map
	for (int code = 0; code < 256; code++)
	{
		KeyboardEvent empty;
		keyboard_set_map(code, &empty, &empty);
	}

	// read lines
	while (fgets(line, sizeof(line), fp))
	{
		// comment
		if (*line == '#')
			continue;

		config_keymap_config(line);
	}

	fclose(fp);
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

static int print_midi_event(char * buff, int buffer_size, int key, KeyboardEvent & e)
{
	char * s = buff;
	char * end = buff + buffer_size;

	s += print_value(s, end - s, key, key_names, ARRAY_COUNT(key_names));
	s += print_value(s, end - s, e.action >> 4, action_names, ARRAY_COUNT(action_names));
	s += print_value(s, end - s, e.action & 0xf, NULL, 0);

	switch (e.action >> 4)
	{
	case 0x8:
	case 0x9:
		s += print_value(s, end - s, e.arg1, note_names, ARRAY_COUNT(note_names));
		if (e.arg2 != 127)
			s += print_value(s, end - s, e.arg2, NULL, 0);
		break;

	case 0xa:
		s += print_value(s, end - s, e.arg1, note_names, ARRAY_COUNT(note_names));
		s += print_value(s, end - s, e.arg2, NULL, 0);
		break;

	case 0xb:
		s += print_value(s, end - s, e.arg1, controller_names, ARRAY_COUNT(controller_names));
		s += print_value(s, end - s, e.arg2, NULL, 0);
		break;

	default:
		s += print_value(s, end - s, e.arg1, NULL, 0);
		s += print_value(s, end - s, e.arg2, NULL, 0);
		s += print_value(s, end - s, e.arg3, NULL, 0);
		break;
	}


	return s - buff;
}

// save keymap
int config_save_keymap(int key, char * buff, int buffer_size)
{
	char * end = buff + buffer_size;
	char * s = buff;

	KeyboardEvent keydown, keyup, defkeyup;
	keyboard_get_map(key, &keydown, &keyup);
	keyboard_default_keyup(&keydown, &defkeyup);

	if (memcmp(&keyup, &defkeyup, sizeof(keyup)) == 0)
	{
		if (keydown.action & 0xf0)
		{
			s += _snprintf(s, end - s, "key");
			s += print_midi_event(s, end - s, key, keydown);
			s += _snprintf(s, end - s, "\r\n");
		}
	}
	else
	{
		if (keydown.action & 0xf0)
		{
			s += _snprintf(s, end - s, "keydown");
			s += print_midi_event(s, end - s, key, keydown);
			s += _snprintf(s, end - s, "\r\n");
		}

		if (keyup.action & 0xf0)
		{
			s += _snprintf(s, end - s, "keyup");
			s += print_midi_event(s, end - s, key, keyup);
			s += _snprintf(s, end - s, "\r\n");
		}
	}

	return s - buff;
}

// reset config
void config_reset()
{
	cfg_instrument_type = 0;
	cfg_instrument_path[0] = 0;
	cfg_output_type = 0;
	cfg_output_device[0] = 0;
	cfg_output_delay = 10;

	for (byte ch = 0; ch < 16; ch++)
	{
		keyboard_set_octshift(ch, 0);
		keyboard_set_velocity(ch, 127);
		keyboard_set_channel(ch, 0);
	}

	midi_set_key_signature(0);
}

// apply config
static int config_apply()
{
	// set keymap
	if (cfg_keymap[0])
		config_load_keymap(cfg_keymap);

	// load instrument
	config_select_instrument(cfg_instrument_type, cfg_instrument_path);

	// open output
	if (config_select_output(cfg_output_type, cfg_output_device))
		cfg_output_device[0] = 0;

	// open midi output and input
	config_select_midi_output(cfg_midi_output);
	config_select_midi_input(cfg_midi_input);
	return 0;
}


// load
int config_load(const char * filename)
{
	char line[256];
	config_get_media_path(line, sizeof(line), filename);

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

			else if (match_word(&s, "channel"))
			{
				uint channel;
				uint value;

				if (match_number(&s, &channel) &&
					match_number(&s, &value))
				{
					keyboard_set_channel(channel, value);
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
			if (match_word(&s, "key"))
			{
				int value;

				if (match_number(&s, &value))
				{
					midi_set_key_signature(value);
				}
			}

			else if (match_word(&s, "output"))
			{
				match_line(&s, cfg_midi_output, sizeof(cfg_midi_output));
			}

			else if (match_word(&s, "input"))
			{
				match_line(&s, cfg_midi_input, sizeof(cfg_midi_input));
			}
		}
	}

	fclose(fp);

	return config_apply();
}

// save
int config_save(const char * filename)
{
	char file_path[256];
	config_get_media_path(file_path, sizeof(file_path), filename);

	FILE * fp = fopen(filename, "w");
	if (!fp)
		return -1;

	if (cfg_instrument_type)
	{
		fprintf(fp, "instrument type %s\n", instrument_type_names[cfg_instrument_type]);
		if (cfg_instrument_path[0])
			fprintf(fp, "instrument path %s\n", cfg_instrument_path);

		fprintf(fp, "instrument showui %d\n", vsti_is_show_editor());
	}


	if (cfg_output_type)
	{
		fprintf(fp, "output type %s\n", output_type_names[cfg_output_type]);

		if (cfg_output_device[0])
			fprintf(fp, "output device %s\n", cfg_output_device);

		fprintf(fp, "output delay %d\n", cfg_output_delay);
	}

	if (cfg_keymap[0])
		fprintf(fp, "keyboard map %s\n", cfg_keymap);

	for (int i = 0; i < 16; i ++)
	{
		if (keyboard_get_octshift(i))
			fprintf(fp, "keyboard shift %d %d\n", i, keyboard_get_octshift(i));

		if (keyboard_get_velocity(i) != 127)
			fprintf(fp, "keyboard velocity %d %d\n", i, keyboard_get_velocity(i));

		if (keyboard_get_channel(i))
			fprintf(fp, "keyboard channel %d %d\n", i, keyboard_get_channel(i));
	}


	if (cfg_midi_output[0])
		fprintf(fp, "midi output %s\n", cfg_midi_output);

	if (cfg_midi_input[0])
		fprintf(fp, "midi input %s\n", cfg_midi_input);

	if (midi_get_key_signature())
		fprintf(fp, "midi key %d\n", midi_get_key_signature());

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
		cfg_instrument_type = type;
		strncpy(cfg_instrument_path, name, sizeof(cfg_instrument_path));
		result = 0;
	}

	else if (type == INSTRUMENT_TYPE_VSTI)
	{
		if (name == NULL || name[0] == '\0')
		{
			vsti_unload_plugin();
			cfg_instrument_path[0] = 0;
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
					cfg_instrument_type = type;
					strncpy(cfg_instrument_path, callback.name, sizeof(cfg_instrument_path));
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
				cfg_instrument_type = type;
				strncpy(cfg_instrument_path, name, sizeof(cfg_instrument_path));
			}
		}
	}

	return result;
}

const char * config_get_instrument_path()
{
	return cfg_instrument_path;
}

// select output
int config_select_output(int type, const char * device)
{
	int result = -1;
	asio_close();
	dsound_close();
	wasapi_close();
	midi_close_output();

	// open output
	switch (type)
	{
	case OUTPUT_TYPE_NONE:
		result = 0;
		break;

	case OUTPUT_TYPE_DSOUND:
		{
			result = dsound_open(device);
			dsound_set_buffer_time(cfg_output_delay);
		}
		break;

	case OUTPUT_TYPE_WASAPI:
		{
			result = wasapi_open(device);
			wasapi_set_buffer_time(cfg_output_delay);
		}
		break;

	case OUTPUT_TYPE_ASIO:
		{
			result = asio_open(device);
		}
		break;
	}

	// success
	if (result == 0)
	{
		cfg_output_type = type;
		strncpy(cfg_output_device, device, sizeof(cfg_output_device));
	}

	return result;
}


// select midi input
int config_select_midi_input(const char * device)
{
	int result = -1;
	if (result = midi_open_input(device))
	{
		cfg_midi_input[0] = 0;
		return result;
	}

	strncpy(cfg_midi_input, device, sizeof(cfg_midi_input));
	return result;
}

// get midi input
const char * config_get_midi_input()
{
	return cfg_midi_input;
}

// select midi output
int config_select_midi_output(const char * device)
{
	int result = -1;
	if (result = midi_open_output(device))
	{
		return result;
	}

	strncpy(cfg_midi_output, device, sizeof(cfg_midi_output));
	return result;
}

// get midi output
const char * config_get_midi_output()
{
	return cfg_midi_output;
}

// get output delay
int config_get_output_delay()
{
	return cfg_output_delay;
}

// get output delay
void config_set_output_delay(int delay)
{
	if (delay < 0)
		delay = 0;
	else if (delay > 500)
		delay = 500;

	cfg_output_delay = delay;

	// open output
	switch (cfg_output_type)
	{
	case OUTPUT_TYPE_NONE:
		break;

	case OUTPUT_TYPE_DSOUND:
		dsound_set_buffer_time(cfg_output_delay);
		break;

	case OUTPUT_TYPE_WASAPI:
		wasapi_set_buffer_time(cfg_output_delay);
		break;

	case OUTPUT_TYPE_ASIO:
		break;
	}
}

// get output type
int config_get_output_type()
{
	return cfg_output_type;
}

// get output device
const char * config_get_output_device()
{
	return cfg_output_device;
}

// get keymap
const char * config_get_keymap()
{
	return cfg_keymap;
}

// set keymap
int config_set_keymap(const char * mapname)
{
	int result;
	if (result = config_load_keymap(mapname))
	{
		cfg_keymap[0] = 0;
	}
	else
	{
		strncpy(cfg_keymap, mapname, sizeof(cfg_keymap));
	}
	return result;
}


// get exe path
void config_get_media_path(char * buff, int buff_size, const char * path)
{
	GetModuleFileNameA(NULL, buff, buff_size);
	char * pathend = strrchr(buff, '\\');
	if (pathend)
	{
#ifdef _DEBUG
		_snprintf(pathend, buff + buff_size - pathend, "\\..\\%s", path);
#else
		_snprintf(pathend, buff + buff_size - pathend, "\\%s", path);
#endif
	}
	else
	{
		strncpy(buff, path, buff_size);
	}
}