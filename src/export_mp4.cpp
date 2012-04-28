#include "pch.h"

#include "export_mp4.h"
#include "gui.h"
#include "song.h"
#include "synthesizer_vst.h"
#include "display.h"

#include "../3rd/mp4v2/mp4.h"
#include "../3rd/libfaac/faac.h"

extern "C" {

#define __X264__
#define HAVE_MMX	1
#define SYS_WINDOWS	1
#define HAVE_STDINT_H 1
#define HAVE_AVS 1
#define HAVE_GPL 1
#define HAVE_THREAD 1
#define HAVE_WIN32THREAD 1
#define HAVE_INTERLACED 1
#define X264_CHROMA_FORMAT 0
#define PTW32_STATIC_LIB 0
#include "../3rd/x264/common/common.h"
}

static bool exporting = false;
static HANDLE export_thread = NULL;

static void show_error(const char *msg)
{
	puts(msg);
}

// playing thread
static DWORD __stdcall export_rendering_thread(void * parameter)
{
	// temp buffer for vsti process
	int samples_per_sec = 44100;
	uint result = -1;
	DWORD input_samples;
	DWORD output_size;
	faacEncHandle faac_encoder = NULL;
	x264_t *x264_encoder = NULL;
	unsigned char *faac_buffer = NULL;
	float *input_buffer = NULL;
	MP4FileHandle file = NULL;
	const char *filename = (const char*)(parameter);

	song_stop_playback();

	// skip 5 seconds
	for (int samples = samples_per_sec * 5; samples > 0; samples -= 32)
	{
		float temp_buffer[2][32];

		// update song
		song_update(1000.0 * (double)32 / (double)samples_per_sec);

		// update effect
		vsti_update_config((float)samples_per_sec, 32);

		// call vsti process func
		vsti_process(temp_buffer[0], temp_buffer[1], 32);
	}

	song_start_playback();


	// x264 encoder param
	x264_param_t param;
	x264_param_default(&param);
	x264_param_default(&param);
    //x264_param_default_preset(&param, "ultrafast", NULL);
	param.i_width = display_get_width();
	param.i_height = display_get_height();
	param.i_csp = X264_CSP_RGB;
	param.i_threads = 1;

	// create x264 encoder
	x264_picture_t picture;
	x264_picture_init(&picture);
	x264_picture_alloc(&picture, X264_CSP_RGB, param.i_width, param.i_height);
	picture.i_type = X264_TYPE_AUTO;
	picture.i_qpplus1 = 0;

	x264_encoder = x264_encoder_open(&param);
	if (!x264_encoder)
	{
		show_error("Can't create x264 encoder.");
		goto cleanup;
	}
	x264_encoder_parameters(x264_encoder, &param);

	// create faac encoder.
	faac_encoder = faacEncOpen(samples_per_sec, 2, &input_samples, &output_size);
	if (faac_encoder == NULL)
	{
		show_error("Failed create faac encoder.");
		goto cleanup;
	}

	// allocate output buffer
	faac_buffer = new unsigned char[output_size];
	if (faac_buffer == NULL)
	{
		show_error("Faild allocate buffer");
		goto cleanup;
	}

	// allocate input buffer
	input_buffer = new float[input_samples];
	if (input_buffer == NULL)
	{
		show_error("Faild allocate buffer");
		goto cleanup;
	}

	// get format.
	faacEncConfigurationPtr faac_format = faacEncGetCurrentConfiguration(faac_encoder);
	faac_format->inputFormat = FAAC_INPUT_FLOAT;
	faac_format->outputFormat = 0; //RAW
	faac_format->mpegVersion = MPEG4;
	faac_format->aacObjectType = LOW;
	faac_format->allowMidside = 1;
	faac_format->useTns = 0;
	faac_format->useLfe = 0;
	faac_format->quantqual = 100;

	if(!faacEncSetConfiguration(faac_encoder, faac_format))
	{
		show_error("Unsupported parameters!");
		goto cleanup;
	}


	file = MP4Create(filename, 0, 0);
	if (file == MP4_INVALID_FILE_HANDLE)
	{
		show_error("Can't create file.");
		goto cleanup;
	}

	MP4SetTimeScale(file, 90000);
	MP4TrackId audio_track = MP4AddAudioTrack(file, samples_per_sec, MP4_INVALID_DURATION, MP4_MPEG4_AUDIO_TYPE);
	MP4SetAudioProfileLevel(file, 0x0F);

    BYTE *ASC = 0;
    DWORD ASCLength = 0;
	faacEncGetDecoderSpecificInfo(faac_encoder, &ASC, &ASCLength);
	MP4SetTrackESConfiguration(file, audio_track, (unsigned __int8 *)ASC, ASCLength);

	MP4TrackId video_track = MP4AddH264VideoTrack(file, 90000, 90000 / 30, param.i_width, param.i_height,
		0x64, //sps[1] AVCProfileIndication
		0x00, //sps[2] profile_compat
		0x1f, //sps[3] AVCLevelIndication
		3); // 4 bytes length before each NAL unit
	MP4SetVideoProfileLevel(file, 0x7F);

	uint frame_size = input_samples / 2;
	uint delay_samples = frame_size;
	uint total_samples = 0;
	uint encoded_samples = 0;
	uint progress = 0;

	double total_time = 0;
	double capture_time = 0;
	double capture_delta = 1.0 / 30.0;

	FILE* fp = fopen("test.264", "wb");
	{
		x264_nal_t *headers;
		int i_nal;
        x264_encoder_headers(x264_encoder, &headers, &i_nal);
		for (int i = 0; i < i_nal; i++) {
			fwrite(headers[i].p_payload, headers[i].i_payload, 1, fp);
		}
	}
	for (;;)
	{
		float temp_buffer[2][32];
		float *input_position = input_buffer;

		// generate frame data
		for (int samples_left = frame_size; samples_left;)
		{
			int samples = samples_left < 32 ? samples_left : 32;
			samples_left -= samples;

			double delta_time = (double)samples / (double)samples_per_sec;

			// update song
			song_update(1000.0 * delta_time);

			// update effect
			vsti_update_config((float)samples_per_sec, samples);

			// call vsti process func
			vsti_process(temp_buffer[0], temp_buffer[1], samples);

			for (int i = 0; i < samples; i++)
			{
				input_position[0] = temp_buffer[0][i] * 32767.0;
				input_position[1] = temp_buffer[1][i] * 32767.0;
				input_position += 2;
			}

			total_time += delta_time;
			if (total_time > capture_time)
			{
				x264_nal_t *nal;
				int i_nal;
				int i_frame_size;
				x264_picture_t pic_out;

				// capture image from display
				display_capture_bitmap24(picture.img.plane[0], picture.img.i_stride[0]);

				// encode image to H.264
				i_frame_size = x264_encoder_encode(x264_encoder, &nal, &i_nal, &picture, &pic_out);
				picture.i_pts++;

				for (int nal_id = 0; nal_id < i_nal; ++nal_id)
				{
					fwrite(nal[nal_id].p_payload, nal[nal_id].i_payload, 1, fp);

					if (nal[nal_id].i_payload >= 4)
					{
						uint size = htonl(nal[nal_id].i_payload - 4);
						memcpy(nal[nal_id].p_payload, &size, 4);
					}
					
					// write samples
					if (!MP4WriteSample(file, video_track, nal[nal_id].p_payload, nal[nal_id].i_payload, MP4_INVALID_DURATION, 0, 1))
					{
						show_error("Encode mp4 error.");
						goto cleanup;
					}
				}

				capture_time += capture_delta;
			}
		}

		total_samples += frame_size;

		if (!song_is_playing())
			input_samples = 0;

		// call the actual encoding routine
		int bytes_encoded = faacEncEncode(faac_encoder, (int32_t *)input_buffer, input_samples, faac_buffer, output_size);
		if (bytes_encoded < 0)
		{
			show_error("Encode aac error.");
			goto cleanup;
		}

		// all done
		if (!song_is_playing() && !bytes_encoded)
		{
			result = 0;
			break;
		}

		// exporting breaked
		if (!gui_is_exporting())
			break;

		if (bytes_encoded > 0)
		{
			uint samples_left = total_samples - encoded_samples + delay_samples;
			MP4Duration dur = samples_left > frame_size ? frame_size : samples_left;
			MP4Duration ofs = encoded_samples > 0 ? 0 : delay_samples;

			if (!MP4WriteSample(file, audio_track, faac_buffer, (DWORD)bytes_encoded, dur, ofs, 1))
			{
				show_error("Encode mp4 error.");
				goto cleanup;
			}

			encoded_samples += dur;
		}

		uint new_progress = 100 * song_get_time() / song_get_length();
		if (new_progress != progress)
		{
			progress = new_progress;
			gui_update_export_progress(progress);
		}
	}

cleanup:
	x264_picture_clean(&picture);
	fclose(fp);

	if (file) MP4Close(file);
	if (faac_encoder) faacEncClose(faac_encoder);
	if (faac_buffer) delete[] faac_buffer;
	if (input_buffer) delete[] input_buffer;
	if (x264_encoder) x264_encoder_close(x264_encoder);

	song_stop_playback();
	gui_close_export_progress();
	return result;
}

// export mp4
int export_mp4(const char *filename)
{
	exporting = true;

	// create input thread
	export_thread = CreateThread(NULL, 0, &export_rendering_thread, (void*)filename, NULL, NULL);

	// show export progress
	gui_show_export_progress();

	// wait for thread end.
	//WaitForSingleObject(export_thread, -1);

	exporting = false;
	return 0;
}

// export rendering
bool export_rendering()
{
	return exporting;
}