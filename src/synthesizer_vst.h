#pragma once;


struct vsti_enum_callback
{
	virtual void operator ()(const char * value) = 0;
};

// load plugin
int vsti_load_plugin(const char * path);

// unload plugin
void vsti_unload_plugin();

// show effect editor
void vsti_show_editor(bool show);

// is editor visible
bool vsti_is_show_editor();

// send midi event
void vsti_send_midi_event(byte a, byte b, byte c, byte d);

// stop output
void vsti_stop_process();

// update config
void vsti_update_config(float samplerate, uint blocksize);

// process
void vsti_process(float * left, float * right, uint buffer_size);

// vst enum plugins
void vsti_enum_plugins(vsti_enum_callback & callback);

// is instrument loaded
bool vsti_is_instrument_loaded();