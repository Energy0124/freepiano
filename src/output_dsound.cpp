#include "pch.h"
#include <dsound.h>
#include <mmreg.h>
#include <mmsystem.h>

#include "output_dsound.h"
#include "synthesizer_vst.h"
#include "gui.h"

// global directsound object
static LPDIRECTSOUND dsound = NULL;

// global primary buffer
static LPDIRECTSOUNDBUFFER primary_buffer = NULL;

// playing event and thread
static HANDLE dsound_thread = NULL;

// output buffer size
static uint buffer_size = 441;


static void write_buffer(short * dst, float * left, float * right, int size)
{
	const float scale = 0x7fff  + .49999f;

	while (size--)
	{
		if (*left > 1) *left = 1;
		else if (*left < -1) *left = -1;

		if (*right > 1) *right = 1;
		else if (*right < -1) *right = -1;

		dst[0] = (short)(scale * (*left));
		dst[1] = (short)(scale * (*right));

		left++;
		right++;
		dst+=2;
	}
}

// playing thread
static DWORD __stdcall dsound_play_thread(void * param)
{
	// primary buffer caps
	DSBCAPS caps;
	caps.dwSize = sizeof(caps);
	primary_buffer->GetCaps(&caps);

	// primary buffer format
	WAVEFORMATEX format;
	primary_buffer->GetFormat(&format, sizeof(format), NULL);

	// timer resolustion
	timeBeginPeriod(1);

	// temp buffer for vsti process
	float output_buffer[2][4096];

	// data write posision
	int write_position = 0;

	// start playing primary buffer
	primary_buffer->Play(0, 0, DSBPLAY_LOOPING);

	while (dsound_thread)
	{
		DWORD play_pos, write_pos;

		// get current position
		primary_buffer->GetCurrentPosition(&play_pos, &write_pos);

		// size left in buffer to write.
		int data_buffer_size = (caps.dwBufferBytes + write_position - write_pos) % caps.dwBufferBytes;

		// write buffer size
		int write_buffer_size = buffer_size * format.nBlockAlign;

		// size left in buffer
		int write_size = write_buffer_size - data_buffer_size;

		// maybe data buffer is underflowed.
		if (write_size < 0)
		{
			write_size = (caps.dwBufferBytes + write_pos + write_buffer_size - write_position) % caps.dwBufferBytes;
		}

		// write data at least 32 samles
		if (write_size >= 32 * format.nBlockAlign)
		{
			//printf("%8d, %8d, %8d, %8d, %8d\n", timeGetTime(),  (caps.dwBufferBytes + write_position + write_size - write_pos) % caps.dwBufferBytes, write_pos, write_size, write_buffer_size);

			// samples
			uint samples = write_size / format.nBlockAlign;

			// call vsti process func
			vsti_update_config((float)format.nSamplesPerSec, 4096);
			vsti_process(output_buffer[0], output_buffer[1], samples);

			// lock primary buffer
			void  *ptr1, *ptr2;
			DWORD size1, size2;
			HRESULT hr = primary_buffer->Lock(write_position, write_size, &ptr1, &size1, &ptr2, &size2, 0);

			// device lost, restore and try again
			if (DSERR_BUFFERLOST == hr)
			{
				primary_buffer->Restore();
				hr = primary_buffer->Lock(write_position, write_size, &ptr1, &size1, &ptr2, &size2, 0);
			}

			// lock succeeded
			if (SUCCEEDED(hr))
			{
				float * left = output_buffer[0];
				float * right = output_buffer[1];

				uint samples1 = size1 / format.nBlockAlign;
				uint samples2 = size2 / format.nBlockAlign;

				if (ptr1) write_buffer((short*)ptr1, left, right, samples1);
				if (ptr2) write_buffer((short*)ptr2, left + samples1, right + samples1, samples2);

				hr = primary_buffer->Unlock(ptr1, size1, ptr2, size2);

				write_position = (write_position + write_size) % caps.dwBufferBytes;
			}

		}
		else
		{
			Sleep(1);
		}
	}

	// stop sound buffer
	primary_buffer->Stop();

	// timer resolustion
	timeEndPeriod(10);

	return 0;
}

// open dsound.
int dsound_open(const char * name)
{
	HRESULT hr;

	// close dsound device
	dsound_close();

	if (name && name[0])
	{
		struct enum_callback
		{
			HRESULT hr;
			const char * name;

			static BOOL CALLBACK func(LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext)
			{
				enum_callback * context = (enum_callback*)lpContext;

				if (_stricmp(context->name, lpcstrDescription) == 0)
				{
					context->hr = DirectSoundCreate(lpGuid, &dsound, NULL);
					return false;
				}

				return true;
			}
		} callback;

		callback.name = name;
		callback.hr = E_FAIL;
		DirectSoundEnumerate(&enum_callback::func, &callback);

		// create dsound object
		if (FAILED(hr = callback.hr))
			goto error;
	}
	else
	{
		// create dsound object
		if (FAILED(DirectSoundCreate(NULL, &dsound, NULL)))
			goto error;
	}

	// wave format used for primary buffer
	WAVEFORMATEX format;
	memset(&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 2;
	format.nSamplesPerSec = 44100;
	format.wBitsPerSample = 16;
	format.nBlockAlign = (format.nChannels * format.wBitsPerSample) / 8;
	format.nAvgBytesPerSec = format.nBlockAlign * format.nSamplesPerSec;
	format.cbSize = sizeof(format);


#if USE_PRIMIARY_WRITE
	// set cooperative level
	if (FAILED(hr = dsound->SetCooperativeLevel(display_get_hwnd(), DSSCL_WRITEPRIMARY)))
		goto error;

	// initialize parameters 
	DSBUFFERDESC desc;
	memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
	desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

	// create primary buffer
	if (FAILED(dsound->CreateSoundBuffer(&desc, &primary_buffer, NULL)))
		goto error;

	// set primary buffer format
	if (FAILED(primary_buffer->SetFormat(&format)))
		goto error;
#else
	// set cooperative level
	if (FAILED(hr = dsound->SetCooperativeLevel(gui_get_window(), DSSCL_PRIORITY)))
		goto error;

	// initialize parameters 
	DSBUFFERDESC desc;
	memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
	desc.dwBufferBytes = 4096 * format.nBlockAlign;
	desc.lpwfxFormat = &format;

	// create primary buffer
	if (FAILED(hr = dsound->CreateSoundBuffer(&desc, &primary_buffer, NULL)))
		goto error;
#endif

	// create input thread
	dsound_thread = CreateThread(NULL, 0, &dsound_play_thread, NULL, NULL, NULL);

	// change thread priority to highest
	SetThreadPriority(dsound_thread, THREAD_PRIORITY_TIME_CRITICAL);

	return S_OK;

error:
	dsound_close();
	return hr;
}

// close dsound device
void dsound_close()
{
	// wait input thread exit
	if (dsound_thread)
	{
		HANDLE thread = dsound_thread;
		dsound_thread = NULL;
		WaitForSingleObject(thread, -1);
		CloseHandle(thread);
	}

	SAFE_RELEASE(primary_buffer);
	SAFE_RELEASE(dsound);
}

// set buffer size
void dsound_set_buffer_time(double time)
{
	if (time > 0)
	{
 		buffer_size = (uint)(44.1 * time);

		if (buffer_size < 32) buffer_size = 32;
		if (buffer_size > 4096) buffer_size = 4000;
	}
}

// enum device
void dsound_enum_device(dsound_enum_callback & callback)
{
	struct EnumDSound
	{
		static BOOL CALLBACK Func(LPGUID guid, LPCSTR desc, LPCSTR module, LPVOID context)
		{
			dsound_enum_callback * callback = (dsound_enum_callback*)context;
			(*callback)(desc);
			return TRUE;
		}
	};

	DirectSoundEnumerateA(&EnumDSound::Func, &callback);
}