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