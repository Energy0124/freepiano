#include "pch.h"

#include "export_mp4.h"
#include "gui.h"
#include "song.h"
#include "synthesizer_vst.h"
#include "display.h"
#include "config.h"
#include "export.h"

#include "../3rd/mp4v2/mp4.h"
#include "../3rd/libfaac/faac.h"

extern "C" {
#include "../3rd/libx264/include/x264.h"
}

static HANDLE export_thread = NULL;

static void show_error(const char *msg) {
  puts(msg);
}

#if 0
static FILE *fp_raw = NULL;
static FILE *fp_264 = NULL;
static FILE *fp_aac = NULL;
static void dump_open(x264_t *encoder) {
  fp_raw = fopen("dump.raw", "wb");
  fp_264 = fopen("dump.264", "wb");
  fp_aac = fopen("dump.aac", "wb");

  if (1) {
    x264_nal_t *headers;
    int i_nal;
    x264_encoder_headers(encoder, &headers, &i_nal);
    for (int i = 0; i < i_nal; i++) {
      fwrite(headers[i].p_payload, headers[i].i_payload, 1, fp_264);
    }
  }
}
static void dump_close() {
  if (fp_raw) fclose(fp_raw);
  if (fp_264) fclose(fp_264);
  if (fp_aac) fclose(fp_aac);
  fp_raw = fp_264 = fp_aac = NULL;
}
static void dump_raw(void *data, size_t size) { fwrite(data, size, 1, fp_raw); }
static void dump_264(void *data, size_t size) { fwrite(data, size, 1, fp_264); }
static void dump_aac(void *data, size_t size) { fwrite(data, size, 1, fp_aac); }
#else
static void dump_open(x264_t *encoder) {}
static void dump_close() {}
static void dump_raw(void *data, size_t size) {}
static void dump_264(void *data, size_t size) {}
static void dump_aac(void *data, size_t size) {}
#endif

typedef struct {
  struct {
    int size_min;
    int next;
    int cnt;
    int idx[17];
    int poc[17];
  } dpb;
  int cnt;
  int cnt_max;
  int *frame;
} h264_dpb_t;

static void DpbInit(h264_dpb_t *p) {
  p->dpb.cnt = 0;
  p->dpb.next = 0;
  p->dpb.size_min = 0;
  p->cnt = 0;
  p->cnt_max = 0;
  p->frame = NULL;
}

static void DpbClean(h264_dpb_t *p) {
  free(p->frame);
}

static void DpbUpdate(h264_dpb_t *p, int is_forced) {
  int i;
  int pos;
  if (!is_forced && p->dpb.cnt < 16)
    return;
  /* find the lowest poc */
  pos = 0;
  for (i = 1; i < p->dpb.cnt; i++) {
    if (p->dpb.poc[i] < p->dpb.poc[pos])
      pos = i;
  }
  //fprintf(stderr, "lowest=%d\n", pos);
  /* save the idx */
  if (p->dpb.idx[pos] >= p->cnt_max) {
    int inc = 1000 + (p->dpb.idx[pos] - p->cnt_max);
    p->cnt_max += inc;
    p->frame = (int *)realloc(p->frame, sizeof(int) * p->cnt_max);
    for (i = 0; i < inc; i++)
      p->frame[p->cnt_max - inc + i] = -1;   /* To detect errors latter */
  }
  p->frame[p->dpb.idx[pos]] = p->cnt++;
  /* Update the dpb minimal size */
  if (pos > p->dpb.size_min)
    p->dpb.size_min = pos;
  /* update dpb */
  for (i = pos; i < p->dpb.cnt - 1; i++) {
    p->dpb.idx[i] = p->dpb.idx[i + 1];
    p->dpb.poc[i] = p->dpb.poc[i + 1];
  }
  p->dpb.cnt--;
}

static void DpbFlush(h264_dpb_t *p) {
  while (p->dpb.cnt > 0)
    DpbUpdate(p, true);
}

static void DpbAdd(h264_dpb_t *p, int poc, int is_idr) {
  if (is_idr)
    DpbFlush(p);
  p->dpb.idx[p->dpb.cnt] = p->dpb.next;
  p->dpb.poc[p->dpb.cnt] = poc;
  p->dpb.cnt++;
  p->dpb.next++;

  DpbUpdate(p, false);
}

static int DpbFrameOffset(h264_dpb_t *p, int idx) {
  if (idx >= p->cnt)
    return 0;
  if (p->frame[idx] < 0)
    return p->dpb.size_min;     /* We have an error (probably broken/truncated bitstream) */
  return p->dpb.size_min + p->frame[idx] - idx;
}

// playing thread
static DWORD __stdcall export_rendering_thread(void *parameter) {
  // temp buffer for vsti process
  const int samples_per_sec = 44100;
  const int frames_per_sec = 30;
  const int mp4_time_scale = 90000;

  uint result = -1;
  DWORD input_samples;
  DWORD output_size;
  faacEncHandle faac_encoder = NULL;
  x264_t *x264_encoder = NULL;
  unsigned char *faac_buffer = NULL;
  float *input_buffer = NULL;
  MP4FileHandle file = NULL;
  const char *filename = (const char *)(parameter);

  song_stop_playback();

  // skip 5 seconds
  for (int samples = samples_per_sec * 5; samples > 0; samples -= 32) {
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
  x264_param_default_preset(&param, "medium", NULL);
  param.i_width = display_get_width();
  param.i_height = display_get_height();
  param.i_csp = X264_CSP_I420;
  param.b_repeat_headers = 0;

  // create x264 encoder
  x264_picture_t picture;
  x264_picture_init(&picture);
  x264_picture_alloc(&picture, param.i_csp, param.i_width, param.i_height);
  picture.i_type = X264_TYPE_AUTO;
  picture.i_qpplus1 = 0;

  h264_dpb_t h264_dpb;
  DpbInit(&h264_dpb);

  x264_encoder = x264_encoder_open(&param);
  if (!x264_encoder) {
    show_error("Can't create x264 encoder.");
    goto done;
  }
  x264_encoder_parameters(x264_encoder, &param);

  // create faac encoder.
  faac_encoder = faacEncOpen(samples_per_sec, 2, &input_samples, &output_size);
  if (faac_encoder == NULL) {
    show_error("Failed create faac encoder.");
    goto done;
  }

  // allocate output buffer
  faac_buffer = new unsigned char[output_size];
  if (faac_buffer == NULL) {
    show_error("Faild allocate buffer");
    goto done;
  }

  // allocate input buffer
  const uint input_buffer_count = 4;
  uint input_buffer_id = 0;
  input_buffer = new float[input_buffer_count * input_samples];
  if (input_buffer == NULL) {
    show_error("Faild allocate buffer");
    goto done;
  }
  memset(input_buffer, 0, input_buffer_count * input_samples * sizeof(float));

  // get format.
  faacEncConfigurationPtr faac_format = faacEncGetCurrentConfiguration(faac_encoder);
  faac_format->inputFormat = FAAC_INPUT_FLOAT;
  faac_format->outputFormat = 0;   //0:RAW
  faac_format->mpegVersion = MPEG4;
  faac_format->aacObjectType = LOW;
  faac_format->allowMidside = 1;
  faac_format->useTns = 0;
  faac_format->useLfe = 0;
  faac_format->quantqual = 100;

  if (!faacEncSetConfiguration(faac_encoder, faac_format)) {
    show_error("Unsupported parameters!");
    goto done;
  }


  file = MP4Create(filename, 0, 0);
  if (file == MP4_INVALID_FILE_HANDLE) {
    show_error("Can't create file.");
    goto done;
  }

  MP4SetTimeScale(file, mp4_time_scale);
  MP4TrackId audio_track = MP4AddAudioTrack(file, samples_per_sec, input_samples / 2, MP4_MPEG4_AUDIO_TYPE);
  MP4SetAudioProfileLevel(file, 0x0F);

  BYTE *ASC = 0;
  DWORD ASCLength = 0;
  faacEncGetDecoderSpecificInfo(faac_encoder, &ASC, &ASCLength);
  MP4SetTrackESConfiguration(file, audio_track, (unsigned __int8 *)ASC, ASCLength);

  MP4TrackId video_track;
  {
    x264_nal_t *nal;
    int i_nal;
    x264_encoder_headers(x264_encoder, &nal, &i_nal);

    uint8_t *sps = nal[0].p_payload;
    video_track = MP4AddH264VideoTrack(file, mp4_time_scale, mp4_time_scale / frames_per_sec, param.i_width, param.i_height,
                                       sps[5], sps[6], sps[7], 3);
    MP4SetVideoProfileLevel(file, 0x7f);
    MP4AddH264SequenceParameterSet(file, video_track, nal[0].p_payload + 4, nal[0].i_payload - 4);
    MP4AddH264PictureParameterSet(file, video_track, nal[1].p_payload + 4, nal[0].i_payload - 4);
  }

  uint frame_size = input_samples / 2;
  uint delay_samples = frame_size;
  uint total_samples = 0;
  uint encoded_samples = 0;
  uint progress = 0;
  uint frame_count = 0;
  uint frame_duration = mp4_time_scale / frames_per_sec;

  double total_time = 0;
  double capture_time = 0;
  double capture_delta = 1.0 / (double)frames_per_sec;

  dump_open(x264_encoder);
  for (;; ) {
    float temp_buffer[2][32];
    float *input_position = input_buffer + input_samples * input_buffer_id;
    input_buffer_id = (input_buffer_id + 1) % input_buffer_count;

    // generate frame data
    for (int samples_left = frame_size; samples_left; ) {
      int samples = samples_left < 32 ? samples_left : 32;
      samples_left -= samples;

      double delta_time = (double)samples / (double)samples_per_sec;

      // update song
      song_update(1000.0 * delta_time);

      // update effect
      vsti_update_config((float)samples_per_sec, samples);

      // call vsti process func
      vsti_process(temp_buffer[0], temp_buffer[1], samples);

      double volume = config_get_output_volume() / 100.0;
      for (int i = 0; i < samples; i++) {
        input_position[0] = temp_buffer[0][i] * 32767.0 * volume;
        input_position[1] = temp_buffer[1][i] * 32767.0 * volume;
        input_position += 2;
      }

      total_time += delta_time;
      if (total_time > capture_time) {
        x264_nal_t *nal;
        int i_nal;
        int i_frame_size;
        x264_picture_t pic_out;

        // capture image from display
        display_capture_bitmap_I420(picture.img.plane, picture.img.i_stride);

        for (int i = 0; i < param.i_height; i++)
          dump_raw(picture.img.plane[0] + i * picture.img.i_stride[0], param.i_width);
        for (int i = 0; i < param.i_height / 2; i++)
          dump_raw(picture.img.plane[1] + i * picture.img.i_stride[1], param.i_width / 2);
        for (int i = 0; i < param.i_height / 2; i++)
          dump_raw(picture.img.plane[2] + i * picture.img.i_stride[2], param.i_width / 2);

        // encode image to H.264
        i_frame_size = x264_encoder_encode(x264_encoder, &nal, &i_nal, &picture, &pic_out);
        picture.i_pts++;

        for (int nal_id = 0; nal_id < i_nal; ++nal_id) {
          uint size = nal[nal_id].i_payload;
          uint8_t *nalu = nal[nal_id].p_payload;

          dump_264(nalu, size);

          nalu[0] = ((size - 4) >> 24) & 0xff;
          nalu[1] = ((size - 4) >> 16) & 0xff;
          nalu[2] = ((size - 4) >> 8) & 0xff;
          nalu[3] = ((size - 4) >> 0) & 0xff;

          // write samples
          MP4Duration dur = mp4_time_scale / frames_per_sec;
          if (!MP4WriteSample(file, video_track, nalu, size, dur, 0, pic_out.b_keyframe != 0)) {
            show_error("Encode mp4 error.");
            goto done;
          }

          bool slice_is_idr = nal[nal_id].i_type == 5;
          DpbAdd(&h264_dpb, pic_out.i_pts, slice_is_idr);
          frame_count++;
        }

        capture_time += capture_delta;
      }
    }

    total_samples += frame_size;

    if (!song_is_playing())
      input_samples = 0;

    // call the actual encoding routine
    int32_t *src = (int32_t *)input_buffer + input_buffer_id * input_samples;
    int bytes_encoded = faacEncEncode(faac_encoder, src, input_samples, faac_buffer, output_size);
    if (bytes_encoded < 0) {
      show_error("Encode aac error.");
      goto done;
    }

    // all done
    if (!song_is_playing() && !bytes_encoded) {
      result = 0;
      break;
    }

    // exporting breaked
    if (!gui_is_exporting())
      break;

    if (bytes_encoded > 0) {
      uint samples_left = total_samples - encoded_samples + delay_samples;
      MP4Duration dur = samples_left > frame_size ? frame_size : samples_left;
      MP4Duration ofs = encoded_samples > 0 ? 0 : delay_samples;

      dump_aac(faac_buffer, bytes_encoded);
      if (!MP4WriteSample(file, audio_track, faac_buffer, (DWORD)bytes_encoded, dur, ofs, 1)) {
        show_error("Encode mp4 error.");
        goto done;
      }

      encoded_samples += dur;
    }

    uint new_progress = 100 * song_get_time() / song_get_length();
    if (new_progress != progress) {
      progress = new_progress;
      gui_update_export_progress(progress);
    }
  }

done:
  DpbFlush(&h264_dpb);
  if (h264_dpb.dpb.size_min > 0) {
    for (uint ix = 0; ix < frame_count; ix++) {
      const int offset = DpbFrameOffset(&h264_dpb, ix);
      MP4SetSampleRenderingOffset(file, video_track, 1 + ix, offset * frame_duration);
    }
  }
  DpbClean(&h264_dpb);

  dump_close();
  x264_picture_clean(&picture);
  if (file) {
    MP4Close(file);
    MP4Optimize(filename);
  }
  if (faac_encoder) faacEncClose(faac_encoder);
  if (faac_buffer) delete[] faac_buffer;
  if (input_buffer) delete[] input_buffer;
  if (x264_encoder) x264_encoder_close(x264_encoder);

  song_stop_playback();
  gui_close_export_progress();
  return result;
}

// export mp4
int export_mp4(const char *filename) {
  export_start();

  // create input thread
  export_thread = CreateThread(NULL, 0, &export_rendering_thread, (void *)filename, NULL, NULL);

  // show export progress
  gui_show_export_progress();

  // wait for thread end.
  //WaitForSingleObject(export_thread, -1);

  export_done();
  return 0;
}
