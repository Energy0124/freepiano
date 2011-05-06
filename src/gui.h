#pragma once

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