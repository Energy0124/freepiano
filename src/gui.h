#pragma once

// delcare type
struct song_info_t;

// init gui system
int gui_init();

// shutdown gui system
void gui_shutdown();

// get main window handle
HWND gui_get_window();

// show gui
void gui_show();

// popup key menu
void gui_popup_keymenu(byte code, int x, int y);

// show song info
int gui_show_song_info();

// get selected key
int gui_get_selected_key();

// show export progress
int gui_show_export_progress();

// update progress
void gui_update_export_progress(int progress);

// close hide progress
void gui_close_export_progress();

// is exporting
bool gui_is_exporting();

// set language
void gui_set_language(int lang);

// notify update
void gui_notify_update(uint version);