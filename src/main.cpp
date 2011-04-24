#include "pch.h"
#include "keyboard.h"
#include "display.h"

#include "config.h"
#include "synthesizer_vst.h"


#ifdef _DEBUG
int main()
#else
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
#endif
{
	// init display
	if (display_init())
	{
		MessageBox(NULL, "error while initialize display.", APP_NAME, MB_OK);
		return 1;
	}

	// show display
	display_show();

	// intialize keyboard
	if (keyboard_init())
	{
		MessageBox(NULL, "error while initialize direct input.", APP_NAME, MB_OK);
		return 1;
	}

	// load default config
	config_load("default.cfg");

	vsti_show_editor(true);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// shutdown keyboard
	keyboard_shutdown();

	// close devices
	config_close();

	// shutdown display
	display_shutdown();

	return 0;
}