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

struct song_event_t
{
	double time;
	byte a;
	byte b;
	byte c;
	byte d;
};

// record buffer (1M events should be enough.)
static song_event_t song_event_buffer[1024 * 1024];
static song_event_t song_temp_buffer[1024 * 1024];

// record or play position
static song_event_t * play_position = NULL;
static song_event_t * record_position = NULL;
static song_event_t * song_end = NULL;
static double song_timer;

static song_info_t song_info;

// -----------------------------------------------------------------------------------------
// record and playback
// -----------------------------------------------------------------------------------------

// add event
static void song_add_event(double time, byte a, byte b, byte c, byte d)
{
	if (record_position)
	{
		record_position->time = time;
		record_position->a = a;
		record_position->b = b;
		record_position->c = c;
		record_position->d = d;

		if (++record_position >= ARRAY_END(song_event_buffer))
			record_position = NULL;

		// just simple set song end to record posision
		song_end = record_position;
	}
}

// init record
static void song_init_record()
{
	song_timer = 0;
	record_position = song_event_buffer;
	play_position = NULL;
	song_end = song_event_buffer;
	song_info.version = 0x01000000;
	song_info.author[0] = 0;
	song_info.title[0] = 0;
	song_info.comment[0] = 0;
	song_info.write_protected = false;
}

// start record
void song_start_record()
{
	song_stop_playback();
	song_stop_record();
	song_init_record();

	// record current configs
	song_add_event(0, 1, 0, midi_get_key_signature(), 0);
	for (int i = 0; i < 16; i++)
	{
		song_add_event(0, 2, i, 0, keyboard_get_octshift(i));
		song_add_event(0, 3, i, 0, keyboard_get_velocity(i));
		song_add_event(0, 4, i, 0, keyboard_get_channel(i));
	}

	// record keymaps
	for (int i = 0; i < 256; i ++)
	{
		KeyboardEvent keydown, keyup;
		keyboard_get_map(i, &keydown, &keyup);
		
		if (keydown.action)
		{
			song_add_event(0, 0, 1, i, 0);
			song_add_event(0, keydown.action, keydown.arg1, keydown.arg2, keydown.arg3);
		}

		if (keyup.action)
		{
			song_add_event(0, 0, 1, i, 1);
			song_add_event(0, keyup.action, keyup.arg1, keyup.arg2, keyup.arg3);
		}

		if (*keyboard_get_label(i))
		{
			const char * label = keyboard_get_label(i);
			int size = strlen(label);

			song_add_event(0, 0, 2, i, size);

			for (int i = 0; i < (size + 3) / 4; i++)
			{
				song_add_event(0, label[0], label[1], label[2], label[3]);
				label += 4;
			}
		}
	}

	// pedal
	song_add_event(0, 0xb0, 0x40, midi_get_controller_value(0x40), 0);
}

// stop record
void song_stop_record()
{
	if (record_position)
	{
		song_add_event(song_timer, 8, 0, 0, 0);
		song_reset_event();
		record_position = NULL;
	}
}

// is recoding
bool song_is_recording()
{
	return record_position != NULL;
}

// start playback
void song_start_playback()
{
	if (song_end)
	{
		song_stop_record();
		song_stop_playback();

		song_timer = 0;
		play_position = song_event_buffer;
		keyboard_clear();
		song_update(0);
	}
}

// stop playback
void song_stop_playback()
{
	song_reset_event();
	play_position = NULL;
}

// is recoding
bool song_is_playing()
{
	return play_position != NULL;
}


// get record length
int song_get_length()
{
	return 0;
}

// allow save
bool song_allow_save()
{
	return song_end != NULL && !song_info.write_protected && !song_is_recording();
}

// allow input
bool song_allow_input()
{
	return play_position == NULL;
}

// song has data
bool song_is_empty()
{
	return song_end == NULL;
}


// get time
int song_get_time()
{
	return (int)song_timer;
}

// update
void song_update(double time_elapsed)
{
	if (GetAsyncKeyState(VK_ESCAPE))
		return;

	if (song_is_playing() || song_is_recording())
		song_timer += time_elapsed;

	// playback
	while (play_position && song_end)
	{
		if (play_position->time <= song_timer)
		{
			// send event to keyboard
			song_send_event(play_position->a, play_position->b, play_position->c, play_position->d);

			if (play_position)
			{
				if (++play_position >= song_end)
				{
					song_stop_playback();
				}
			}
		}
		else break;

	}
}

song_info_t * song_get_info()
{
	return &song_info;
}

// -----------------------------------------------------------------------------------------
// event parser
// -----------------------------------------------------------------------------------------

// dynamic mapping
static byte keyboard_map_key_code = 0;
static byte keyboard_map_key_type = 0;
static byte keyboard_label_key_code = 0;
static byte keyboard_label_key_size = 0;
static char keyboard_label_text[256];

// wrap value
static int wrap_value(int value, int min_v, int max_v)
{
	if (value < min_v) value = max_v;
	if (value > max_v) value = min_v;
	return value;
}

// clamp value
static int clamp_value(int value, int min_v, int max_v)
{
	if (value < min_v) value = min_v;
	if (value > max_v) value = max_v;
	return value;
}


// keyboard event map
static void keyboard_event_map(int code, int type)
{
	keyboard_map_key_code = code;
	keyboard_map_key_type = type;
}

// keyboard event label
static void keyboard_event_label(int code, int length)
{
	keyboard_label_key_code = code;
	keyboard_label_key_size = length;
	keyboard_label_text[0] = 0;
}

// event message
void song_send_event(byte a, byte b, byte c, byte d, bool record)
{
	// record event
	if (record)
	{
		// HACK: don't record playback control commands
		if (a != 6 && a != 7 && a != 8)
			song_add_event(song_timer, a, b, c, d);
	}

	// special events
	if (a == 0)
	{
		switch (b)
		{
		case 0:	keyboard_key_event(c, d); break;
		case 1:	keyboard_event_map(c, d); break;
		case 2: keyboard_event_label(c, d); break;
		}

		return;
	}

	// setting a key label
	if (keyboard_label_key_size)
	{
		char * ch = keyboard_label_text + strlen(keyboard_label_text);
		char data[4] = { a, b, c, d };

		for (int i = 0; i < 4; i++)
		{
			ch[i] = data[i];
			ch[i + 1] = 0;

			if (--keyboard_label_key_size == 0)
			{
				keyboard_set_label(keyboard_label_key_code, keyboard_label_text);
				break;
			}
		}
		return;
	}

	// mapping a key.
	if (keyboard_map_key_code)
	{
		KeyboardEvent keydown, keyup;

		// get previous keymap
		keyboard_get_map(keyboard_map_key_code, &keydown, &keyup);

		switch (keyboard_map_key_type)
		{
		case 0:
			keydown.action = a;
			keydown.arg1 = b;
			keydown.arg2 = c;
			keydown.arg3 = d;
			break;

		case 1:
			keyup.action = a;
			keyup.arg1 = b;
			keyup.arg2 = c;
			keyup.arg3 = d;
			break;
		}

		// set keymap
		keyboard_set_map(keyboard_map_key_code, &keydown, &keyup);
		keyboard_map_key_code = 0;
		return;
	}

	// midi events
	if (a >= 0x80)
	{
		midi_send_event(a, b, c, d);
		return;
	}

	// keyboard control events
	switch (a)
	{
	case 1:	// key signature
		{
			int value = midi_get_key_signature();

			switch (b)
			{
			case 0: value = c; break;
			case 1: value = wrap_value(value + (char)c, -4, 7); break;
			case 2: value = wrap_value(value - (char)c, -4, 7); break;
			}

			midi_set_key_signature(value);
		}
		break;

	case 2:	// oct shift
		{
			int ch = b;
			int value = keyboard_get_octshift(ch);

			switch (c)
			{
			case 0: value = d; break;
			case 1: value = wrap_value(value + (char)d, -1, 1); break;
			case 2: value = wrap_value(value - (char)d, -1, 1); break;
			}

			keyboard_set_octshift(ch, value);
		}
		break;

	case 3:	// velocity
		{
			int ch = b;
			int value = keyboard_get_velocity(ch);

			switch (c)
			{
			case 0: value = d; break;
			case 1: value = clamp_value(value + (char)d, 0, 127); break;
			case 2: value = clamp_value(value - (char)d, 0, 127); break;
			}

			keyboard_set_velocity(ch, value);
		}
		break;

	case 4: // channel
		{
			int ch = b;
			int value = keyboard_get_channel(ch);

			switch (c)
			{
			case 0: value = d; break;
			case 1: value = clamp_value(value + (char)d, 0, 15); break;
			case 2: value = clamp_value(value - (char)d, 0, 15); break;
			}

			keyboard_set_channel(ch, value);
		}
		break;

	case 5: // volume
		{
			int value = config_get_output_volume();

			switch (b)
			{
			case 0: value = c; break;
			case 1: value = clamp_value(value + (char)c, 0, 100); break;
			case 2: value = clamp_value(value - (char)c, 0, 100); break;
			}

			config_set_output_volume(value);
		}
		break;

	case 6:	// play
		if (song_allow_input())
			song_start_playback();
		break;

	case 7:	// record
		if (song_allow_input())
		{
			if (song_is_recording())
				song_stop_record();
			else
				song_start_record();
		}
		break;

	case 8:	// stop
		if (song_allow_input())
		{
			if (song_is_recording())
				song_stop_record();

			if (song_is_playing())
				song_stop_playback();
		}
		break;
	}
}

// reset
void song_reset_event()
{
	// exit key map mode
	keyboard_map_key_code = 0;

	// exit key label mode
	keyboard_label_key_size = 0;

	// reset keyboard
	keyboard_reset();

	// reset midi
	midi_reset();
}



// -----------------------------------------------------------------------------------------
// load and save functions
// -----------------------------------------------------------------------------------------

static void read(void * buff, int size, FILE * fp)
{
	int ret = fread(buff, 1, size, fp);
	if (ret != size)
		throw -1;
}

static void write(const void * buff, int size, FILE * fp)
{
	int ret = fwrite(buff, 1, size, fp);
	if (ret != size)
		throw -1;
}

static void read_string(char * buff, size_t buff_size, FILE * fp)
{
	uint count = 0;

	read(&count, sizeof(count), fp);

	if (count < buff_size)
	{
		read(buff, count, fp);
		buff[count] = 0;
	}
	else
	{
		throw -1;
	}
}

static void write_string(const char * buff, FILE * fp)
{
	uint count = strlen(buff);

	write(&count, sizeof(count), fp);
	write(buff, count, fp);
}

static void read_idp_string(char * buff, size_t buff_size, FILE * fp)
{
	byte count = 0;
	read(&count, sizeof(count), fp);

	if (count < buff_size)
	{
		read(buff, buff_size, fp);
		buff[count] = 0;
	}
	else
	{
		throw -1;
	}
}

// open lyt
int song_open_lyt(const char * filename)
{
	song_close();

	FILE * fp = fopen(filename, "rb");
	if (!fp)
		return -1;

	try
	{
		char header[16];

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
		struct keymap_t
		{
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
		song_add_event(0, 1, 0, map.midi_shift, 0);
		song_add_event(0, 2, 0, 0, map.shift_left);
		song_add_event(0, 2, 1, 0, map.shift_right);
		song_add_event(0, 3, 0, 0, map.velocity_left);
		song_add_event(0, 3, 1, 0, map.velocity_right);

		// push down padel
		if (map.voice1 > 0 || map.voice2 > 0)
			song_add_event(0, 0xb0, 0x40, 127, 0);

		if (song_info.version > 1)
		{
			static const byte keycode[] = 
			{
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

			// clear keymap
			for (int i = 0; i < 255; i++)
			{
				KeyboardEvent empty;
				keyboard_set_map(i, &empty, &empty);
			}

			// set keymap
			for (int i = 0; i < 107; i++)
			{
				struct key_t
				{
					int enabled;
					byte note;
					byte shift;
					byte octive;
					byte channel;

					char unknown[0x44];
				};

				key_t key;
				read(&key, sizeof(key), fp);

				if (key.enabled && key.note)
				{
					KeyboardEvent keydown, keyup;
					keydown.action = 0x90 | (key.channel & 0xf);
					keydown.arg1 = 12 + key.octive * 12 + notes[key.note % 8] + shifts[key.shift % 3];
					keydown.arg2 = 127;

					// add keymap event
					song_add_event(0, 0, 1, keycode[i], 0);
					song_add_event(0, keydown.action, keydown.arg1, keydown.arg2, keydown.arg3);
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


		for (int i = 0; i < event_count; i ++)
		{
			int time;
			int unknown;
			byte type;
			int key;

			read(&time, sizeof(time), fp);
			read(&unknown, sizeof(unknown), fp);

			read(&type, sizeof(type), fp);
			read(&key, sizeof(key), fp);

			switch (type)
			{	
			case 1: song_add_event(time, 0, 0, key, 1); break; // keydown
			case 2: song_add_event(time, 0, 0, key, 0); break; // keyup
			case 3: song_add_event(time, 2, 1, 0, key); break; // set oct shift
			case 4: song_add_event(time, 2, 0, 0, key); break; // set oct shift
			case 5: song_add_event(time, 1, 0, key, 0); break; // set midi oct shift
			case 18:	break; // drum
			case 19:	break; // maybe song end.
			default:
				printf("unknown type : %d\t%d\t%d\n", time, type, key);
			}
		}

		record_position = NULL;
		fclose(fp);


		// mark song protected
		song_info.write_protected = true;

		// display song info
		gui_show_song_info();
	}
	catch (int err)
	{
		song_end = NULL;
		fclose(fp);
		return err;
	}

	return 0;
}


// close
void song_close()
{
	song_stop_record();
	song_stop_playback();
	song_end = NULL;
}

// open song
int song_open(const char * filename)
{
	song_close();

	FILE * fp = fopen(filename, "rb");
	if (!fp)
		return -1;

	try
	{
		char magic[sizeof("FreePianoSong")];
		char instrument[256];

		// magic
		read(magic, sizeof("FreePianoSong"), fp);
		if (memcmp(magic, "FreePianoSong", sizeof("FreePianoSong")) != 0)
			throw -1;

		// version
		read(&song_info.version, sizeof(song_info.version), fp);
		if (song_info.version != 0x01000000)
			throw -2;

		// start record
		song_init_record();

		// song info
		read_string(song_info.title, sizeof(song_info.title), fp);
		read_string(song_info.author, sizeof(song_info.author), fp);
		read_string(song_info.comment, sizeof(song_info.comment), fp);

		// instrument
		read_string(instrument, sizeof(instrument), fp);


		// read events
		uint temp_size;
		read(&temp_size, sizeof(temp_size), fp);
		if (temp_size > sizeof(song_temp_buffer))
			throw 1;

		read(song_temp_buffer, temp_size, fp);

		uint data_size = sizeof(song_event_buffer);
		uncompress((byte*)song_event_buffer, (uLongf*)&data_size, (Bytef*)song_temp_buffer, temp_size);

		song_end = record_position = song_event_buffer + data_size / sizeof(song_event_t);

		record_position = NULL;

		fclose(fp);

		// mark song protected
		song_info.write_protected = true;

		// display song info
		gui_show_song_info();

		return 0;
	}
	catch (int err)
	{
		song_end = NULL;
		fclose(fp);
		return err;
	}
}


// save song
int song_save(const char * filename)
{
	song_stop_record();

	if (song_end)
	{
		FILE * fp = fopen(filename, "wb");
		if (!fp)
			return -1;
		try
		{
			// magic
			write("FreePianoSong", sizeof("FreePianoSong"), fp);

			// version
			song_info.version = 0x01000000;
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
			uint temp_size = sizeof(song_temp_buffer);

			compress((Bytef*)song_temp_buffer, (uLongf*)&temp_size, (byte*)song_event_buffer, (song_end - song_event_buffer) * sizeof(song_event_t));

			write(&temp_size, sizeof(temp_size), fp);
			write(song_temp_buffer, temp_size, fp);

			fclose(fp);
			return 0;
		}
		catch (int err)
		{
			fclose(fp);
			return err;
		}
	}
	return -1;
}