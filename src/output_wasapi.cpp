#include "pch.h"
#include <mmreg.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include "output_wasapi.h"
#include "synthesizer_vst.h"
#include "display.h"

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
	while (size--)
	{
		dst[0] = *left;
		dst[1] = *right;

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

	// initialize effect
	vsti_start_process((float)pwfx->nSamplesPerSec, 32);

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


	WAVEFORMATEXTENSIBLE * format1 = (WAVEFORMATEXTENSIBLE*)pwfx;
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

	// create input thread
	// initialize
	if (FAILED(hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, hnsRequestedDuration, 0, &format.Format, NULL)))
		goto error;

	// get render client
	if (FAILED(hr = client->GetService(IID_IAudioRenderClient, (void**)&render_client)))
		goto error;

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
	CoUninitialize();
}

// set buffer size
void wasapi_set_buffer_time(double time)
{
	if (time > 0)
	{
		buffer_size = (uint)(44100.0 * time);

		if (buffer_size < 32) buffer_size = 32;
		if (buffer_size > 4096) buffer_size = 4096;
	}
}