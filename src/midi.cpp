#include "pch.h"
#include "midi.h"
#include "synthesizer_vst.h"
#include "display.h"

static HMIDIOUT midi_out_device = NULL;
static HMIDIIN midi_in_device = NULL;


// c key of midi
static char octshift = 0;


// open output device
int midi_open_output(const char * name)
{
	uint device_id = -1;
	midi_close_output();

	if (name)
	{
		for (uint i = 0; i < midiOutGetNumDevs(); i++)
		{
			MIDIOUTCAPS caps;

			// get device caps
			if (midiOutGetDevCaps(i, &caps, sizeof(caps)))
				continue;

			if (_stricmp(name, caps.szPname) == 0)
			{
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
void midi_close_output()
{
	if (midi_out_device)
	{
		midiOutClose(midi_out_device);
		midi_out_device = NULL;
	}
}

static void CALLBACK midi_input_callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	printf("%x, %08x, %08x\n", wMsg, dwParam1, dwParam2);
	if (wMsg == MIM_DATA)
	{
		uint data = (uint)dwParam1;
		midi_send_event(data >> 0, data >> 8, data >> 16, data >> 24);
	}
}

// open input device
int midi_open_input(const char * name)
{
	uint device_id = -1;
	midi_close_input();

	if (name)
	{
		for (uint i = 0; i < midiInGetNumDevs(); i++)
		{
			MIDIINCAPS caps;

			// get device caps
			if (midiInGetDevCaps(i, &caps, sizeof(caps)))
				continue;

			if (_stricmp(name, caps.szPname) == 0)
			{
				device_id = i;
				break;
			}
		}
	}

	if (midiInOpen(&midi_in_device, device_id, (DWORD_PTR)&midi_input_callback, 0, CALLBACK_FUNCTION))
		return -1;

	printf("open input success");
	midiInStart(midi_in_device);
	return 0;
}

// close input device
void midi_close_input()
{
	if (midi_in_device)
	{
		midiInClose(midi_in_device);
		midi_in_device = NULL;
	}
}


// send event
void midi_send_event(byte data1, byte data2, byte data3, byte data4)
{
	// modify event
	midi_modify_event(data1, data2, data3, data4, octshift, 127);

	// display midi event
	display_midi_event(data1, data2, data3, data4);

	// send event to output device
	if (midi_out_device)
	{
		midiOutShortMsg(midi_out_device, data1 | (data2 << 8) | (data3 << 16) | (data4 << 24));
	}
	else
	{
		// send midi event to vst plugin
		vsti_send_midi_event(data1, data2, data3, data4);
	}
}

// set octshift
void midi_set_octshift(char shift)
{
	octshift = shift;
}

// get oct shift
char midi_get_octshift()
{
	return octshift;
}

static inline byte clamp(int data, byte min = 0, byte max = 127)
{
	if (data < min) return min;
	if (data > max) return max;
	return data;
}

// modify event
void midi_modify_event(byte & data1, byte & data2, byte & data3, byte & data4, char shift, byte velocity)
{
	// process event
	switch (data1 >> 4)
	{
	case 0x8:
	case 0x9:
		data2 = clamp((int)data2 + shift);
		data3 = clamp((int)data3 * velocity / 127);
		break;
	}

	// change channel
	data1 &= 0xf0;

}