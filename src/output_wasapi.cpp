#include "pch.h"
#include <mmreg.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mbctype.h>

#include "output_wasapi.h"
#include "synthesizer_vst.h"
#include "display.h"
#include "song.h"
#include "config.h"

// pkey
static const PROPERTYKEY PKEY_Device_FriendlyName = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20,  0x67,  0xd1,  0x46,  0xa8,  0x50,  0xe0 } }, 14 };

// global wasapi client
static IAudioClient * client = NULL;

// global renderer client
static IAudioRenderClient * render_client = NULL;

// playing event and thread
static HANDLE wasapi_thread = NULL;

// format
static WAVEFORMATEX * pwfx = NULL;

// output buffer size
static uint buffer_size = 441;

// class id and interface id
static const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
static const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
static const IID IID_IAudioClient = __uuidof(IAudioClient);
static const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

// write buffer
static void write_buffer(float * dst, float * left, float * right, int size)
{
	float volume = config_get_output_volume() / 100.f;

	while (size--)
	{
		dst[0] = *left * volume;
		dst[1] = *right * volume;

		left++;
		right++;
		dst+=2;
	}
}

// playing thread
static DWORD __stdcall wasapi_play_thread(void * param)
{
	HRESULT hr;
	uint bufferFrameCount;
	BYTE * data;

	// Get the actual size of the allocated buffer.
	if (FAILED(hr = client->GetBufferSize(&bufferFrameCount)))
		return 0;

	// timer resolustion
	timeBeginPeriod(1);

	// temp buffer for vsti process
	float output_buffer[2][4096];

	// start playing
	hr = client->Start();

	while (wasapi_thread)
	{
		uint numFramesPadding;

		 // See how much buffer space is available.
		if (SUCCEEDED(hr = client->GetCurrentPadding(&numFramesPadding)))
		{
			if (numFramesPadding <= buffer_size)
			{
				uint numFramesAvailable = bufferFrameCount - numFramesPadding;
				uint numFramesProcess = 32;

				// write data at least 32 samles
				if (numFramesAvailable >= numFramesProcess)
				{
					//printf("write %d\n", numFramesAvailable);

					// update song
					song_update(1000.0 * (double)32 / (double)pwfx->nSamplesPerSec);

					// update effect
					vsti_update_config((float)pwfx->nSamplesPerSec, 32);

					// call vsti process func
					vsti_process(output_buffer[0], output_buffer[1], numFramesProcess);


					// Grab the entire buffer for the initial fill operation.
					if (SUCCEEDED(hr = render_client->GetBuffer(numFramesAvailable, &data)))
					{
						// write buffer
						write_buffer((float*)data, output_buffer[0], output_buffer[1], numFramesProcess);

						// release buffer
						render_client->ReleaseBuffer(numFramesProcess, 0);

						// continue processing
						continue;
					}
				}
			}
		}

		Sleep(1);
	}

	// stop playing
	client->Stop();

	// stop vsti process
	vsti_stop_process();

	return 0;
}

// open wasapi.
int wasapi_open(const char * name)
{
	HRESULT hr;
	IMMDeviceEnumerator * enumerator = NULL;
	IMMDevice * device = NULL;
	REFERENCE_TIME hnsRequestedDuration = 1000000;

	// close wasapi device
	wasapi_close();

	// initialize com
	if (FAILED(hr = CoInitialize(NULL)))
		goto error;

	// create enumerator
	if (FAILED(hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
		IID_IMMDeviceEnumerator, (void**)&enumerator)))
		goto error;

	// get default device
	if (FAILED(hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)))
		goto error;

	// activate
	if (FAILED(hr = device->Activate(IID_IAudioClient, CLSCTX_ALL,NULL, (void**)&client)))
		goto error;

	// get mix format
	if (FAILED(hr = client->GetMixFormat(&pwfx)))
		goto error;


	WAVEFORMATEXTENSIBLE format;
	format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE);
	format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	format.Format.nChannels = 2;
	format.Format.nSamplesPerSec = 44100;
	format.Format.wBitsPerSample = 32;
	format.Format.nBlockAlign = (format.Format.nChannels * format.Format.wBitsPerSample) / 8;
	format.Format.nAvgBytesPerSec = format.Format.nBlockAlign * format.Format.nSamplesPerSec;
	format.dwChannelMask = 3;
	format.Samples.wReserved = 32;
	format.Samples.wSamplesPerBlock = 32;
	format.Samples.wValidBitsPerSample = 32;
	format.SubFormat = 	KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

	// initialize
	if (FAILED(hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, hnsRequestedDuration, 0, &format.Format, NULL)))
		goto error;

	// get render client
	if (FAILED(hr = client->GetService(IID_IAudioRenderClient, (void**)&render_client)))
		goto error;

	// create input thread
	wasapi_thread = CreateThread(NULL, 0, &wasapi_play_thread, NULL, NULL, NULL);

	// change thread priority to highest
	SetThreadPriority(wasapi_thread, THREAD_PRIORITY_TIME_CRITICAL);

	SAFE_RELEASE(device);
	SAFE_RELEASE(enumerator);
	return 0;

error:
	SAFE_RELEASE(device);
	SAFE_RELEASE(enumerator);
	wasapi_close();
	return hr;
}

// close wasapi device
void wasapi_close()
{
	// wait input thread exit
	if (wasapi_thread)
	{
		HANDLE thread = wasapi_thread;
		wasapi_thread = NULL;
		WaitForSingleObject(thread, -1);
		CloseHandle(thread);
	}

	CoTaskMemFree(pwfx);
	SAFE_RELEASE(render_client);
	SAFE_RELEASE(client);
	//CoUninitialize();
	pwfx = NULL;
}

// set buffer size
void wasapi_set_buffer_time(double time)
{
	if (time > 0)
	{
		buffer_size = (uint)(44.1 * time);

		if (buffer_size < 32) buffer_size = 32;
		if (buffer_size > 4096) buffer_size = 4096;
	}
}

// enum device
void wasapi_enum_device(wasapi_enum_callback & callback)
{
	HRESULT hr;
	IMMDeviceEnumerator * enumerator = NULL;
	IMMDeviceCollection * devices = NULL;
	IPropertyStore * props = NULL;

	// initialize com
	if (FAILED(hr = CoInitialize(NULL)))
		goto error;

	// create enumerator
	if (FAILED(hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
		IID_IMMDeviceEnumerator, (void**)&enumerator)))
		goto error;


	if (FAILED(hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices)))
		goto error;

	uint device_count = 0;
	if (SUCCEEDED(devices->GetCount(&device_count)))
	{
		for (uint i = 0; i < device_count; i++)
		{
			IMMDevice * device = NULL;
			if (SUCCEEDED(devices->Item(i, &device)))
			{
				if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props)))
				{
					PROPVARIANT varName;

					// Initialize container for property value.
					PropVariantInit(&varName);

					// Get the endpoint's friendly-name property.
					if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName)))
					{
						char buff[256];
						WideCharToMultiByte(_getmbcp(), 0, varName.pwszVal, -1, buff, sizeof(buff), NULL, NULL);
						callback(buff);
					}

					PropVariantClear(&varName);

				}
			}
			SAFE_RELEASE(device);
		}
	}

error:
	SAFE_RELEASE(props);
	SAFE_RELEASE(devices);
	SAFE_RELEASE(enumerator);
	CoUninitialize();

}