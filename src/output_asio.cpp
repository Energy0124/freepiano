#include "pch.h"
#include <math.h>

#include "asio/asiosys.h"
#include "asio/asiolist.h"
#include "asio/iasiodrv.h"
#include "asio/asiodrivers.h"
#include "output_asio.h"
#include "keyboard.h"
#include "synthesizer_vst.h"
#include "song.h"

static IASIO* driver = NULL;
static int driver_index = -1;
static char driver_name[64] = {0};
static AsioDriverList driver_list;

enum {
	// number of input and outputs supported by the host application
	// you can change these to higher or lower values
	kMaxInputChannels = 32,
	kMaxOutputChannels = 32
};

// internal data storage
typedef struct DriverInfo
{
	ASIODriverInfo driverInfo;

	long           inputChannels;
	long           outputChannels;

	long           minSize;
	long           maxSize;
	long           preferredSize;
	long           granularity;

	ASIOSampleRate sampleRate;

	bool           postOutput;

	long           inputLatency;
	long           outputLatency;

	long inputBuffers;	// becomes number of actual created input buffers
	long outputBuffers;	// becomes number of actual created output buffers
	ASIOBufferInfo bufferInfos[kMaxInputChannels + kMaxOutputChannels]; // buffer info's
	ASIOChannelInfo channelInfos[kMaxInputChannels + kMaxOutputChannels]; // channel info's

	// data is converted to double floats for easier use, however 64 bit integer can be used, too
	double         nanoSeconds;
	double         samples;
	double         tcSamples;	// time code samples

	ASIOTime       tInfo;			// time info state
	unsigned long  sysRefTime;      // system reference time, when bufferSwitch() was called

	// Signal the end of processing in this example
	bool           stopped;
} DriverInfo;


static DriverInfo driver_info = {0};
static ASIOCallbacks callbacks;

// close driver
static void close_driver()
{
#if 0
	if (driver_index >= 0)
		driver_list.asioCloseDriver(driver_index);

	driver_index = -1;
#endif
}

// open driver
static int open_driver(const char * name)
{
#if 0
	// close driver
	close_driver();

	// foreach driver
	for (long i = 0; i < driver_list.asioGetNumDev(); i++)
	{
		if (driver_list.asioGetDriverName(i, driver_name, 32))
			continue;

		if (strcmp(name, driver_name))
			continue;

		if(driver_list.asioOpenDriver(i, (void **)&driver) == 0)
		{
			driver_index = i;
			return 0;
		}
	}

	driver_name[0] = 0;
	return -1;
#endif
	extern bool loadAsioDriver(const char *name);
	return !loadAsioDriver(name);
}

static int create_asio_buffers()
{
	// create buffers for all inputs and outputs of the card with the 
	// preferredSize from ASIOGetBufferSize() as buffer size
	long i;
	ASIOError result;

	// fill the bufferInfos from the start without a gap
	ASIOBufferInfo *info = driver_info.bufferInfos;

	// prepare inputs (Though this is not necessaily required, no opened inputs will work, too
	if (driver_info.inputChannels > kMaxInputChannels)
		driver_info.inputBuffers = kMaxInputChannels;
	else
		driver_info.inputBuffers = driver_info.inputChannels;
	for(i = 0; i < driver_info.inputBuffers; i++, info++)
	{
		info->isInput = ASIOTrue;
		info->channelNum = i;
		info->buffers[0] = info->buffers[1] = 0;
	}

	// prepare outputs
	if (driver_info.outputChannels > kMaxOutputChannels)
		driver_info.outputBuffers = kMaxOutputChannels;
	else
		driver_info.outputBuffers = driver_info.outputChannels;
	for (i = 0; i < driver_info.outputBuffers; i++, info++)
	{
		info->isInput = ASIOFalse;
		info->channelNum = i;
		info->buffers[0] = info->buffers[1] = 0;
	}

	// create and activate buffers
	result = ASIOCreateBuffers(driver_info.bufferInfos,
		driver_info.inputBuffers + driver_info.outputBuffers,
		driver_info.preferredSize, &callbacks);

	if (result == ASE_OK)
	{
		// now get all the buffer details, sample word length, name, word clock group and activation
		for (i = 0; i < driver_info.inputBuffers + driver_info.outputBuffers; i++)
		{
			driver_info.channelInfos[i].channel = driver_info.bufferInfos[i].channelNum;
			driver_info.channelInfos[i].isInput = driver_info.bufferInfos[i].isInput;
			result = ASIOGetChannelInfo(&driver_info.channelInfos[i]);
			if (result != ASE_OK)
				break;
		}

		if (result == ASE_OK)
		{
			// get the input and output latencies
			// Latencies often are only valid after ASIOCreateBuffers()
			// (input latency is the age of the first sample in the currently returned audio block)
			// (output latency is the time the first sample in the currently returned audio block requires to get to the output)
			result = ASIOGetLatencies(&driver_info.inputLatency, &driver_info.outputLatency);
		}
	}

	return result;
}

#if NATIVE_INT64
	#define ASIO64toDouble(a)  (a)
#else
	const double twoRaisedTo32 = 4294967296.;
	#define ASIO64toDouble(a)  ((a).lo + (a).hi * twoRaisedTo32)
#endif

static unsigned long get_sys_reference_time()
{
	return timeGetTime();
}

static ASIOTime *callback_buffer_switch_timeinfo(ASIOTime *timeInfo, long index, ASIOBool processNow)
{	// the actual processing callback.
	// Beware that this is normally in a seperate thread, hence be sure that you take care
	// about thread synchronization. This is omitted here for simplicity.
	static long processedSamples = 0;

	// store the timeInfo for later use
	driver_info.tInfo = *timeInfo;

	// get the time stamp of the buffer, not necessary if no
	// synchronization to other media is required
	if (timeInfo->timeInfo.flags & kSystemTimeValid)
		driver_info.nanoSeconds = ASIO64toDouble(timeInfo->timeInfo.systemTime);
	else
		driver_info.nanoSeconds = 0;

	if (timeInfo->timeInfo.flags & kSamplePositionValid)
		driver_info.samples = ASIO64toDouble(timeInfo->timeInfo.samplePosition);
	else
		driver_info.samples = 0;

	if (timeInfo->timeCode.flags & kTcValid)
		driver_info.tcSamples = ASIO64toDouble(timeInfo->timeCode.timeCodeSamples);
	else
		driver_info.tcSamples = 0;

	// get the system reference time
	driver_info.sysRefTime = get_sys_reference_time();

	// buffer size in samples
	long buffSize = driver_info.preferredSize;


	static float output_buffer[2][4096];

	// update 
	song_update(1000.0 * (double)driver_info.preferredSize / (double)driver_info.sampleRate);


	// call vsti process func
	vsti_update_config((float)driver_info.sampleRate, driver_info.preferredSize);
	vsti_process(output_buffer[0], output_buffer[1], buffSize);

//#if WINDOWS && _DEBUG
//	// a few debug messages for the Windows device driver developer
//	// tells you the time when driver got its interrupt and the delay until the app receives
//	// the event notification.
//	static double last_samples = 0;
//	printf ("diff: %d / %d ms / %d ms / %d samples                 \n", driver_info.sysRefTime - (long)(driver_info.nanoSeconds / 1000000.0), driver_info.sysRefTime, (long)(driver_info.nanoSeconds / 1000000.0), (long)(driver_info.samples - last_samples));
//	last_samples = driver_info.samples;
//#endif


	// perform the processing
	for (int i = 0; i < driver_info.inputBuffers + driver_info.outputBuffers; i++)
	{
		if (driver_info.bufferInfos[i].isInput == false)
		{
			// OK do processing for the outputs only
			switch (driver_info.channelInfos[i].type)
			{
			case ASIOSTInt16LSB:
				{
					double scale = 0x7fff  + .49999;

					short * b = (short*)driver_info.bufferInfos[i].buffers[index];
					float * o = output_buffer[driver_info.channelInfos[i].channel & 1];

					for (int j = 0; j < buffSize; j++)
					{
						if (*o > 1) *o = 1;
						else if (*o < -1) *o = -1;

						*b = (int)(scale * (*o));

						b++;
						o++;
					}
				}
				break;

			case ASIOSTInt24LSB:		// used for 20 bits as well
				memset (driver_info.bufferInfos[i].buffers[index], 0, buffSize * 3);
				break;

			case ASIOSTInt32LSB:
				{
					double scale = 0x7fffffff  + .49999;

					int * b = (int*)driver_info.bufferInfos[i].buffers[index];
					float * o = output_buffer[driver_info.channelInfos[i].channel & 1];

					for (int j = 0; j < buffSize; j++)
					{
						if (*o > 1) *o = 1;
						else if (*o < -1) *o = -1;

						*b = (int)(scale * (*o));

						b++;
						o++;
					}
				}

				break;
			case ASIOSTFloat32LSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
				memset (driver_info.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;
			case ASIOSTFloat64LSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
				memset (driver_info.bufferInfos[i].buffers[index], 0, buffSize * 8);
				break;

				// these are used for 32 bit data buffer, with different alignment of the data inside
				// 32 bit PCI bus systems can more easily used with these
			case ASIOSTInt32LSB16:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32LSB18:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32LSB20:		// 32 bit data with 20 bit alignment
			case ASIOSTInt32LSB24:		// 32 bit data with 24 bit alignment
				memset (driver_info.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;

			case ASIOSTInt16MSB:
				memset (driver_info.bufferInfos[i].buffers[index], 0, buffSize * 2);
				break;
			case ASIOSTInt24MSB:		// used for 20 bits as well
				memset (driver_info.bufferInfos[i].buffers[index], 0, buffSize * 3);
				break;
			case ASIOSTInt32MSB:
				memset (driver_info.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;
			case ASIOSTFloat32MSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
				memset (driver_info.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;
			case ASIOSTFloat64MSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
				memset (driver_info.bufferInfos[i].buffers[index], 0, buffSize * 8);
				break;

				// these are used for 32 bit data buffer, with different alignment of the data inside
				// 32 bit PCI bus systems can more easily used with these
			case ASIOSTInt32MSB16:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32MSB18:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32MSB20:		// 32 bit data with 20 bit alignment
			case ASIOSTInt32MSB24:		// 32 bit data with 24 bit alignment
				memset (driver_info.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;
			}
		}
	}

	// finally if the driver supports the ASIOOutputReady() optimization, do it here, all data are in place
	if (driver_info.postOutput)
		ASIOOutputReady();

	processedSamples += buffSize;

	return 0L;
}

void callback_buffer_switch(long index, ASIOBool processNow)
{	// the actual processing callback.
	// Beware that this is normally in a seperate thread, hence be sure that you take care
	// about thread synchronization. This is omitted here for simplicity.

	// as this is a "back door" into the bufferSwitchTimeInfo a timeInfo needs to be created
	// though it will only set the timeInfo.samplePosition and timeInfo.systemTime fields and the according flags
	ASIOTime  timeInfo;
	memset (&timeInfo, 0, sizeof (timeInfo));

	// get the time stamp of the buffer, not necessary if no
	// synchronization to other media is required
	if(ASIOGetSamplePosition(&timeInfo.timeInfo.samplePosition, &timeInfo.timeInfo.systemTime) == ASE_OK)
		timeInfo.timeInfo.flags = kSystemTimeValid | kSamplePositionValid;

	callback_buffer_switch_timeinfo (&timeInfo, index, processNow);
}


void callback_samplerate_changed(ASIOSampleRate sRate)
{
	// do whatever you need to do if the sample rate changed
	// usually this only happens during external sync.
	// Audio processing is not stopped by the driver, actual sample rate
	// might not have even changed, maybe only the sample rate status of an
	// AES/EBU or S/PDIF digital input at the audio device.
	// You might have to update time/sample related conversion routines, etc.
}

long callback_asio_message(long selector, long value, void* message, double* opt)
{
	// currently the parameters "value", "message" and "opt" are not used.
	long ret = 0;
	switch(selector)
	{
		case kAsioSelectorSupported:
			if(value == kAsioResetRequest
			|| value == kAsioEngineVersion
			|| value == kAsioResyncRequest
			|| value == kAsioLatenciesChanged
			// the following three were added for ASIO 2.0, you don't necessarily have to support them
			|| value == kAsioSupportsTimeInfo
			|| value == kAsioSupportsTimeCode
			|| value == kAsioSupportsInputMonitor)
				ret = 1L;
			break;
		case kAsioResetRequest:
			// defer the task and perform the reset of the driver during the next "safe" situation
			// You cannot reset the driver right now, as this code is called from the driver.
			// Reset the driver is done by completely destruct is. I.e. ASIOStop(), ASIODisposeBuffers(), Destruction
			// Afterwards you initialize the driver again.
			driver_info.stopped;  // In this sample the processing will just stop
			ret = 1L;
			break;
		case kAsioResyncRequest:
			// This informs the application, that the driver encountered some non fatal data loss.
			// It is used for synchronization purposes of different media.
			// Added mainly to work around the Win16Mutex problems in Windows 95/98 with the
			// Windows Multimedia system, which could loose data because the Mutex was hold too long
			// by another thread.
			// However a driver can issue it in other situations, too.
			ret = 1L;
			break;
		case kAsioLatenciesChanged:
			// This will inform the host application that the drivers were latencies changed.
			// Beware, it this does not mean that the buffer sizes have changed!
			// You might need to update internal delay data.
			ret = 1L;
			break;
		case kAsioEngineVersion:
			// return the supported ASIO version of the host application
			// If a host applications does not implement this selector, ASIO 1.0 is assumed
			// by the driver
			ret = 2L;
			break;
		case kAsioSupportsTimeInfo:
			// informs the driver wether the asioCallbacks.bufferSwitchTimeInfo() callback
			// is supported.
			// For compatibility with ASIO 1.0 drivers the host application should always support
			// the "old" bufferSwitch method, too.
			ret = 1;
			break;
		case kAsioSupportsTimeCode:
			// informs the driver wether application is interested in time code info.
			// If an application does not need to know about time code, the driver has less work
			// to do.
			ret = 0;
			break;
	}
	return ret;
}

// open
int asio_open(const char * driver_name)
{
	// open driver
	if (open_driver(driver_name))
		return 1;

	// initialize the driver
	if (ASIOInit (&driver_info.driverInfo) != ASE_OK)
		return 2;

	// get channel info
	if (ASIOGetChannels(&driver_info.inputChannels, &driver_info.outputChannels) != ASE_OK)
		return 3;

	// get the usable buffer sizes
	if (ASIOGetBufferSize(&driver_info.minSize, &driver_info.maxSize, &driver_info.preferredSize, &driver_info.granularity) != ASE_OK)
		return 4;

	// get the currently selected sample rate
	if (ASIOGetSampleRate(&driver_info.sampleRate) != ASE_OK)
		return 5;

	if (driver_info.sampleRate <= 0.0 || driver_info.sampleRate > 96000.0)
	{
		// Driver does not store it's internal sample rate, so set it to a know one.
		// Usually you should check beforehand, that the selected sample rate is valid
		// with ASIOCanSampleRate().
		if(ASIOSetSampleRate(44100.0) != ASE_OK)
			return 6;

		if(ASIOGetSampleRate(&driver_info.sampleRate) == ASE_OK)
			return 7;
	}

	// check wether the driver requires the ASIOOutputReady() optimization
	// (can be used by the driver to reduce output latency by one block)
	if(ASIOOutputReady() == ASE_OK)
		driver_info.postOutput = true;
	else
		driver_info.postOutput = false;

	// set up the asioCallback structure and create the ASIO data buffer
	callbacks.bufferSwitch = &callback_buffer_switch;
	callbacks.sampleRateDidChange = &callback_samplerate_changed;
	callbacks.asioMessage = &callback_asio_message;
	callbacks.bufferSwitchTimeInfo = &callback_buffer_switch_timeinfo;
	if (create_asio_buffers())
		return 8;

	// start asio
	if (ASIOStart())
		return 9;

	return 0;
}


// close
void asio_close()
{
	ASIOStop();
	ASIODisposeBuffers();
	ASIOExit();
	close_driver();
}

// enum device
void asio_enum_device(asio_enum_callback & callback)
{
	LPASIODRVSTRUCT drv = driver_list.lpdrvlist;

	for (int i = 0; i < driver_list.numdrv; i++)
	{
		if (drv)
		{
			callback(drv->drvname);
			drv = drv->next;
		}
	}
}