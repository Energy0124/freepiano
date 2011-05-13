#include "pch.h"

#include <dinput.h>
#include <Shlwapi.h>

#include "song.h"
#include "display.h"
#include "midi.h"
#include "keyboard.h"
#include "config.h"

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

// record or play position
static song_event_t * play_position = NULL;
static song_event_t * record_position = NULL;
static song_event_t * song_end = NULL;
static double song_timer;

static char song_title[256];
static char song_author[256];
static char song_description[256];
static bool song_protected = false;

// start record
void song_start_record()
{
	song_timer = 0;
	record_position = song_event_buffer;
	play_position = NULL;
	song_end = song_event_buffer;
	song_protected = false;
}

// stop record
void song_stop_record()
{
	record_position = NULL;
}

// is recoding
bool song_is_recording()
{
	return record_position != NULL;
}

// start playback
void song_start_playback()
{
	song_timer = 0;
	play_position = song_event_buffer;
}

// stop playback
void song_stop_playback()
{
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
	return song_end != NULL && !song_protected && !song_is_recording();
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
			byte a = play_position->a;
			byte b = play_position->b;
			byte c = play_position->c;
			byte d = play_position->d;

			if ((a & 0xf0) < 0x80)
			{
				keyboard_send_event(a, b, c, d);
			}
			else
			{
				midi_send_event(a, b, c, d);
			}

			if (++play_position > song_end)
			{
				play_position = NULL;
			}
		}
		else break;

	}
}

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

// add event
void song_record_event(byte a, byte b, byte c, byte d)
{
	song_add_event(song_timer, a, b, c, d);
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
		buff[count + 1] = 0;
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
		int version;

		// read magic
		read(header, 16, fp);
		if (strcmp(header, "iDreamPianoSong") != 0)
			throw -1;

		// read header
		read(&version, sizeof(version), fp);
		read_idp_string(song_title, 39, fp);
		read_idp_string(song_author, 19, fp);
		read_idp_string(song_description, 255, fp);

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

		midi_set_key_signature(map.midi_shift);
		keyboard_set_velocity(0, map.velocity_left);
		keyboard_set_velocity(1, map.velocity_right);
		keyboard_set_octshift(0, map.shift_left);
		keyboard_set_octshift(1, map.shift_right);

		// start record
		song_start_record();

		// push down right padel
		song_add_event(0, 0xb0, 0x40, 127, 0);

		if (version > 1)
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
				DIK_APOSTROPHE,
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

					keyboard_default_keyup(&keydown, &keyup);
					keyboard_set_map(keycode[i], &keydown, &keyup);
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
			case 1: song_add_event(time, 0, key, 1, 0); break; // keydown
			case 2: song_add_event(time, 0, key, 0, 0); break; // keyup
			case 3: song_add_event(time, 2, 1, key, 0); break; // set oct shift
			case 4: song_add_event(time, 2, 0, key, 0); break; // set oct shift
			case 5: song_add_event(time, 1, key, 0, 0); break; // set midi oct shift
			case 18:	break; // drum
			case 19:	break; // maybe song end.
			default:
				printf("unknown type : %d\t%d\t%d\n", time, type, key);
			}
		}

		song_stop_record();
		song_protected = true;
		fclose(fp);
	}
	catch (int err)
	{
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
		uint version;

		// magic
		read(magic, sizeof("FreePianoSong"), fp);
		if (memcmp(magic, "FreePianoSong", sizeof("FreePianoSong")) != 0)
			throw -1;

		// version
		read(&version, sizeof(version), fp);
		if (version != 0x01000000)
			throw -2;

		// instrument
		read_string(instrument, sizeof(instrument), fp);

		//// midi key
		//char midi_key;
		//read(&midi_key, sizeof(midi_key), fp);
		//midi_set_key_signature(midi_key);

		//// keyboard info
		//for (int i = 0; i < 16; i++)
		//{
		//	byte velocity;
		//	char octshift;
		//	byte channel;

		//	read(&velocity, sizeof(velocity), fp);
		//	read(&octshift, sizeof(octshift), fp);
		//	read(&channel, sizeof(channel), fp);

		//	keyboard_set_velocity(i, velocity);
		//	keyboard_set_octshift(i, octshift);
		//	keyboard_set_channel(i, channel);
		//}

		//// keymap
		//for (int i = 0; i < 256; i++)
		//{
		//	KeyboardEvent keydown, keyup;
		//	read(&keydown, sizeof(keydown), fp);
		//	read(&keyup, sizeof(keyup), fp);
		//	keyboard_set_map(i, &keydown, &keyup);
		//}

		// start record
		song_start_record();

		// events
		uint event_count;
		read(&event_count, sizeof(event_count), fp);
		for (uint i = 0; i < event_count; i++)
		{
			song_event_t e;
			read(&e, sizeof(e), fp);
			song_add_event(e.time, e.a, e.b, e.c, e.d);
		}

		song_stop_record();
		song_protected = true;
		fclose(fp);
		return 0;
	}
	catch (int err)
	{
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
			int version = 0x01000000;

			// magic
			write("FreePianoSong", sizeof("FreePianoSong"), fp);

			// version
			write(&version, sizeof(version), fp);

			// instrument
			{
				char buff[256];
				strncpy(buff, PathFindFileName(config_get_instrument_path()), sizeof(buff));
				PathRenameExtension(buff, "");
				write_string(buff, fp);
			}

			// write events
			uint event_count = song_end - song_event_buffer;
			write(&event_count, sizeof(event_count), fp);
			for (uint i = 0; i < event_count; i++)
			{
				write(&song_event_buffer[i], sizeof(song_event_t), fp);
			}

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