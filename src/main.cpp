#include "pch.h"
#include "keyboard.h"
#include "display.h"
#include "config.h"
#include "gui.h"
#include "song.h"

#ifdef _DEBUG
int main()
#else
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
#endif
{
	// config init
	if (config_init())
	{
		MessageBox(NULL, "error while initialize config.", APP_NAME, MB_OK);
		return 1;
	}

	// init gui
	if (gui_init())
	{
		MessageBox(NULL, "error while initialize gui.", APP_NAME, MB_OK);
		return 1;
	}

	// init display
	if (display_init(gui_get_window()))
	{
		MessageBox(NULL, "error while initialize display.", APP_NAME, MB_OK);
		return 1;
	}

	// intialize keyboard
	if (keyboard_init())
	{
		MessageBox(NULL, "error while initialize direct input.", APP_NAME, MB_OK);
		return 1;
	}

	// show gui
	gui_show();

	// load default config
	config_load("freepiano.cfg");

	// open lyt
	//song_open_lyt("test3.lyt");
	//song_start_playback();

	MSG msg;
	while (GetMessage(&msg, NULL, NULL, NULL))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	config_save("freepiano.cfg");

	// shutdown keyboard
	keyboard_shutdown();

	// shutdown config
	config_shutdown();

	// shutdown display
	display_shutdown();

	return 0;
}