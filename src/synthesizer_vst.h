#pragma once;


// load plugin
int vsti_load_plugin(const char * path);

// unload plugin
void vsti_unload_plugin();

// show effect editor
void vsti_show_editor(bool show);

// send midi event
void vsti_send_midi_event(byte a, byte b, byte c, byte d);

// start output
int vsti_start_process(float samplerate, uint blocksize);

// stop output
void vsti_stop_process();

// process
void vsti_process(float * left, float * right, uint buffer_size);
