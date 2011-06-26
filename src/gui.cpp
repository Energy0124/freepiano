#include "pch.h"
#include <Shlwapi.h>
#include <CommCtrl.h>
#include <windowsx.h>

#include "gui.h"
#include "config.h"
#include "midi.h"
#include "output_dsound.h"
#include "output_wasapi.h"
#include "output_asio.h"
#include "synthesizer_vst.h"
#include "display.h"
#include "keyboard.h"
#include "song.h"
#include "../res/resource.h"

#pragma comment(lib, "Shlwapi.lib")

// enable vistual style.
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// constants
static const int default_client_width = 760;
static const int default_client_height = 340;

// -----------------------------------------------------------------------------------------
// config window
// -----------------------------------------------------------------------------------------
static HWND setting_hwnd = NULL;
static HWND setting_tab = NULL;
static HWND setting_page = NULL;

static INT_PTR CALLBACK settings_midi_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			HWND input_list = GetDlgItem(hWnd, IDC_MIDI_INPUT_LIST);
			HWND output_list = GetDlgItem(hWnd, IDC_MIDI_OUTPUT_LIST);

			struct enum_callback : midi_enum_callback
			{
				void operator ()(const char * value)
				{
					ListBox_AddString(listbox, value);
				}

				HWND listbox;
			};

			// build input list
			ListBox_AddString(input_list, "（无）");

			enum_callback callback;
			callback.listbox = input_list;
			midi_enum_input(callback);

			if (ListBox_SelectString(input_list, 0, config_get_midi_input()) < 0)
				ListBox_SetCurSel(input_list, 0);
			
			// build output list
			ListBox_AddString(output_list, "Microsoft MIDI Mapper");

			callback.listbox = output_list;
			midi_enum_output(callback);

			if (ListBox_SelectString(output_list, 0, config_get_midi_output()) < 0)
				ListBox_SetCurSel(output_list, 0);
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_MIDI_INPUT_LIST:
			switch (HIWORD(wParam)) 
			{ 
			case LBN_SELCHANGE:
				{
					HWND hwndList = GetDlgItem(hWnd, IDC_MIDI_INPUT_LIST);

					// get current select
					int cursel = ListBox_GetCurSel(hwndList);

					char buff[256];
					buff[0] = 0;

					if (cursel)
					{
						int len = ListBox_GetTextLen(hwndList, cursel);

						if (len < sizeof(buff))
							ListBox_GetText(hwndList, cursel, buff);
					}

					config_select_midi_input(buff);
				}
				break;
			}
			break;

		case IDC_MIDI_OUTPUT_LIST:
			switch (HIWORD(wParam)) 
			{ 
			case LBN_SELCHANGE:
				{
					HWND hwndList = GetDlgItem(hWnd, IDC_MIDI_OUTPUT_LIST);

					// get current select
					int cursel = ListBox_GetCurSel(hwndList);

					char buff[256];
					buff[0] = 0;

					if (cursel)
					{
						int len = ListBox_GetTextLen(hwndList, cursel);

						if (len < sizeof(buff))
							ListBox_GetText(hwndList, cursel, buff);
					}

					config_select_midi_output(buff);
				}
				break;
			}
			break;
		}
		break;
	}

	return 0;
}

static INT_PTR CALLBACK settings_audio_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static char output_delay_text_format[256];
	static char output_volume_text_format[256];

	static const char * output_types[] = 
	{
		"NONE",
		"DirectSound",
		"WASAPI",
		"ASIO",
	};

	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			HWND output_list = GetDlgItem(hWnd, IDC_OUTPUT_LIST);

			// output device list
			struct callback : dsound_enum_callback, wasapi_enum_callback, asio_enum_callback
			{
				void operator ()(const char * value)
				{
					char buff[256];

					_snprintf(buff, sizeof(buff), "%s: %s", output_types[type], value);
					ComboBox_AddString(list, buff);
					if (config_get_output_type() == type)
					{
						if (_stricmp(value, config_get_output_device()) == 0)
						{
							ComboBox_SetCurSel(list, ComboBox_GetCount(list) - 1);
							selected = true;
						}
					}

					count ++;
				}

				void operator ()(const char * value, void * device)
				{
					operator()(value);
				}

				callback(int type, HWND list)
					: type(type)
					, list(list)
					, selected(false)
					, count(0)
				{
				}

				~callback()
				{
					if (config_get_output_type() == type && !selected)
					{
						if (count)
							ComboBox_SetCurSel(list, ComboBox_GetCount(list) - count);
					}
				}

				HWND list;
				int type;
				bool selected;
				int count;
			};

			// dsound output
			ComboBox_SetItemHeight(output_list, 0, 16);
			ComboBox_AddString(output_list, "(自动选择)");
			ComboBox_SetCurSel(output_list, 0);
			dsound_enum_device(callback(OUTPUT_TYPE_DSOUND, output_list));
			wasapi_enum_device(callback(OUTPUT_TYPE_WASAPI, output_list));
			asio_enum_device(callback(OUTPUT_TYPE_ASIO, output_list));

			// output delay
			GetDlgItemText(hWnd, IDC_OUTPUT_DELAY_TEXT, output_delay_text_format, sizeof(output_delay_text_format));
			GetDlgItemText(hWnd, IDC_OUTPUT_VOLUME_TEXT, output_volume_text_format, sizeof(output_volume_text_format));

			char temp[256];
			_snprintf(temp, sizeof(temp), output_delay_text_format, config_get_output_delay());
			SetDlgItemText(hWnd, IDC_OUTPUT_DELAY_TEXT, temp);

			_snprintf(temp, sizeof(temp), output_volume_text_format, config_get_output_volume());
			SetDlgItemText(hWnd, IDC_OUTPUT_VOLUME_TEXT, temp);

			// output delay slider
			HWND output_delay = GetDlgItem(hWnd, IDC_OUTPUT_DELAY);

			SendMessage(output_delay, TBM_SETRANGE, 
				(WPARAM) TRUE,                   // redraw flag 
				(LPARAM) MAKELONG(0, 80));		 // min. & max. positions 

			SendMessage(output_delay, TBM_SETPOS, 
				(WPARAM) TRUE,						 // redraw flag 
				(LPARAM) config_get_output_delay()); // min. & max. positions 

			
			// output volume slider
			HWND output_volume = GetDlgItem(hWnd, IDC_OUTPUT_VOLUME);

			SendMessage(output_volume, TBM_SETRANGE, 
				(WPARAM) TRUE,                   // redraw flag 
				(LPARAM) MAKELONG(0, 100));		 // min. & max. positions 

			SendMessage(output_volume, TBM_SETPOS, 
				(WPARAM) TRUE,						 // redraw flag 
				(LPARAM) config_get_output_volume()); // min. & max. positions 
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_OUTPUT_LIST:
			switch (HIWORD(wParam)) 
			{
			case CBN_SELCHANGE:
				{
					int result = 1;
					char temp[256];
					GetDlgItemText(hWnd, IDC_OUTPUT_LIST, temp, sizeof(temp));

					for (int i = 0; i < ARRAY_COUNT(output_types); i++)
					{
						int len = strlen(output_types[i]);
						if (_strnicmp(output_types[i], temp, len) == 0)
						{
							result = config_select_output(i, temp + len + 2);
							break;
						}
					}

					if (result)
					{
						config_select_output(OUTPUT_TYPE_AUTO, "");
						ComboBox_SetCurSel(GetDlgItem(hWnd, IDC_OUTPUT_LIST), 0);
					}

				}
				break;
			}
			break;
		}
		break;

	case WM_HSCROLL:
		{
			HWND output_delay = GetDlgItem(hWnd, IDC_OUTPUT_DELAY);
			HWND output_volume = GetDlgItem(hWnd, IDC_OUTPUT_VOLUME);

			if (output_delay == (HWND)lParam)
			{
				int pos = SendMessage(output_delay, TBM_GETPOS, 0, 0);

				// update output delay
				config_set_output_delay(pos);

				char temp[256];
				_snprintf(temp, sizeof(temp), output_delay_text_format, config_get_output_delay());
				SetDlgItemText(hWnd, IDC_OUTPUT_DELAY_TEXT, temp);
			}
			else if (output_volume == (HWND)lParam)
			{
				int pos = SendMessage(output_volume, TBM_GETPOS, 0, 0);

				// update output delay
				config_set_output_volume(pos);

				char temp[256];
				_snprintf(temp, sizeof(temp), output_volume_text_format, config_get_output_volume());
				SetDlgItemText(hWnd, IDC_OUTPUT_VOLUME_TEXT, temp);
			}
		}
	}

	return 0;
}

static LRESULT CALLBACK EditSubProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (uMsg)
	{
	case WM_CHAR:
		if (wParam == VK_RETURN)
		{
			SetFocus(NULL);
			SetFocus(hWnd);
			return 0;
		}
		break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static INT_PTR CALLBACK settings_keyboard_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static const char * keyboard_shifts[] =
	{
		"0", "+1", "-1",
	};

	switch (uMsg)
	{
	case WM_INITDIALOG:
		for (int channel = 0; channel < 2; channel ++)
		{
			char buff[256];

			// velocity edit
			_snprintf(buff, sizeof(buff), "%d", config_get_key_velocity(channel));
			HWND edit = GetDlgItem(hWnd, IDC_KEY_VELOCITY1 + channel);
			SetDlgItemText(hWnd, IDC_KEY_VELOCITY1 + channel, buff);


			// velocity slider
			HWND velocity_slider = GetDlgItem(hWnd, IDC_KEY_VELOCITY_SLIDER1 + channel);
			SendMessage(velocity_slider, TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) MAKELONG(0, 127));
			SendMessage(velocity_slider, TBM_SETPOS, (WPARAM) TRUE, (LPARAM) config_get_key_velocity(channel));

			// shift
			HWND octshift = GetDlgItem(hWnd, IDC_KEY_SHIFT1 + channel);
			ComboBox_ResetContent(octshift);
			for (int i = 0; i < ARRAY_COUNT(keyboard_shifts); i++)
			{
				ComboBox_AddString(octshift, keyboard_shifts[i]);
				if (atoi(keyboard_shifts[i]) == config_get_key_octshift(channel))
					ComboBox_SetCurSel(octshift, i);
			}

			// channel
			HWND keychannel = GetDlgItem(hWnd, IDC_KEY_CHANNEL1 + channel);
			ComboBox_ResetContent(keychannel);
			for (int i = 0; i < 16; i++)
			{
				char buff[256];
				_snprintf(buff, sizeof(buff), "%d", i);
				ComboBox_AddString(keychannel, buff);
				if (i == config_get_key_channel(channel))
					ComboBox_SetCurSel(keychannel, i);
			}
		}
		break;

	case WM_HSCROLL:
		for (int channel = 0; channel < 2; channel ++)
		{
			HWND velocity_slider = GetDlgItem(hWnd, IDC_KEY_VELOCITY_SLIDER1 + channel);

			if (velocity_slider == (HWND)lParam)
			{
				int pos = SendMessage(velocity_slider, TBM_GETPOS, 0, 0);

				config_set_key_velocity(channel, pos);

				char temp[256];
				_snprintf(temp, sizeof(temp), "%d", config_get_key_velocity(channel));
				SetDlgItemText(hWnd, IDC_KEY_VELOCITY1 + channel, temp);
			}
		}
		break;

	case WM_COMMAND:
		for (int channel = 0; channel < 2; channel ++)
		{
			if (LOWORD(wParam) == IDC_KEY_VELOCITY1 + channel)
			{
				switch (HIWORD(wParam)) 
				{
				case EN_SETFOCUS:
					{
						PostMessage(GetDlgItem(hWnd, IDC_KEY_VELOCITY1 + channel), EM_SETSEL, 0, -1);
						return 1;
					}
					break;

				case EN_KILLFOCUS:
					{
						char temp[256];
						GetDlgItemText(hWnd, IDC_KEY_VELOCITY1 + channel, temp, sizeof(temp));

						int value = 0;
						if (sscanf(temp, "%d", &value) == 1)
						{
							if (value < 0) value = 0;
							if (value > 127) value = 127;

							config_set_key_velocity(channel, value);
						}

						value = config_get_key_velocity(channel);
						SendMessage(GetDlgItem(hWnd, IDC_KEY_VELOCITY_SLIDER1 + channel), TBM_SETPOS, 1, value);
						_snprintf(temp, sizeof(temp), "%d", value);
						SetDlgItemText(hWnd, IDC_KEY_VELOCITY1 + channel, temp);
					}
					break;
				}
			}

			else if (LOWORD(wParam) == IDC_KEY_SHIFT1 + channel)
			{
				switch (HIWORD(wParam)) 
				{
				case CBN_SELCHANGE:
					{
						char temp[256];
						GetDlgItemText(hWnd, IDC_KEY_SHIFT1 + channel, temp, sizeof(temp));

						config_set_key_octshift(channel, atoi(temp));
					}
					break;
				}
				break;
			}

			else if (LOWORD(wParam) == IDC_KEY_CHANNEL1 + channel)
			{
				switch (HIWORD(wParam)) 
				{
				case CBN_SELCHANGE:
					{
						char temp[256];
						GetDlgItemText(hWnd, IDC_KEY_CHANNEL1 + channel, temp, sizeof(temp));

						config_set_key_channel(channel, atoi(temp));
					}
					break;
				}
				break;
			}
		}
		break;
	}

	return 0;
}

static INT_PTR CALLBACK settings_keymap_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	struct helpers
	{
		static void update_content(HWND edit)
		{
			uint buff_size = 1024 * 1024;
			char * buff = new char[buff_size];

			// save keymap config
			config_save_keymap(buff, buff_size);

			uint tabstops = 46;
			LockWindowUpdate(edit);
			int scroll = GetScrollPos(edit, SB_VERT);
			Edit_SetTabStops(edit, 1, &tabstops);
			Edit_SetText(edit, buff);
			Edit_Scroll(edit, scroll, 0);
			LockWindowUpdate(NULL);

			delete[] buff;
		}

		static void apply_content(HWND edit)
		{
			uint buff_size = 1024 * 1024;
			char * buff = new char[buff_size];

			Edit_GetText(edit, buff, buff_size);
			config_parse_keymap(buff);

			delete[] buff;
		}
	};

	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			HWND output_content = GetDlgItem(hWnd, IDC_MAP_CONTENT);

			helpers::update_content(output_content);
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_MAP_SAVE:
			helpers::update_content(GetDlgItem(hWnd, IDC_MAP_CONTENT));
			break;

		case IDC_MAP_APPLY:
			HWND output_content = GetDlgItem(hWnd, IDC_MAP_CONTENT);

			helpers::apply_content(output_content);
			helpers::update_content(output_content);
			break;
		}
		break;
	}

	return 0;
}

static INT_PTR CALLBACK settings_gui_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			CheckDlgButton(hWnd, IDC_GUI_ENABLE_RESIZE, config_get_enable_resize_window());
			CheckDlgButton(hWnd, IDC_GUI_ENABLE_HOTKEY, !config_get_enable_hotkey());
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_GUI_ENABLE_RESIZE:
			config_set_enable_resize_window(IsDlgButtonChecked(hWnd, IDC_GUI_ENABLE_RESIZE) != 0);
			break;

		case IDC_GUI_ENABLE_HOTKEY:
			config_set_enable_hotkey(!IsDlgButtonChecked(hWnd, IDC_GUI_ENABLE_HOTKEY));
			break;
		}
		break;
	}

	return 0;
}

static struct setting_page_t
{
	int dialog;
	DLGPROC proc;
	HTREEITEM item;
}
setting_pages[] = 
{
	{ IDD_SETTING_AUDIO,	settings_audio_proc },
	{ IDD_SETTING_MIDI,		settings_midi_proc },
	{ IDD_SETTING_KEYBOARD,	settings_keyboard_proc },
	{ IDD_SETTING_KEYMAP,	settings_keymap_proc },
	{ IDD_SETTING_GUI,		settings_gui_proc },
};

static void add_setting_page(HWND list, char * text, int page_id)
{
	TVINSERTSTRUCT tvins;

	tvins.item.mask = TVIF_TEXT | TVIF_PARAM; 
	tvins.item.pszText = text; 
	tvins.item.cchTextMax = 0;

	for (int i = 0; i < ARRAY_COUNT(setting_pages); i++)
	{
		if (setting_pages[i].dialog == page_id)
		{
			tvins.item.lParam = i;
		}
	}

	tvins.hInsertAfter = NULL;
	tvins.hParent = TVI_ROOT;

	setting_pages[tvins.item.lParam].item = TreeView_InsertItem(list, &tvins);
}

static INT_PTR CALLBACK settings_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			HWND setting_list = GetDlgItem(hWnd, IDC_SETTING_LIST);
			add_setting_page(setting_list, "音频", IDD_SETTING_AUDIO);
			add_setting_page(setting_list, "MIDI", IDD_SETTING_MIDI);
			add_setting_page(setting_list, "演奏设置", IDD_SETTING_KEYBOARD);
			add_setting_page(setting_list, "键盘脚本", IDD_SETTING_KEYMAP);
			add_setting_page(setting_list, "界面设置", IDD_SETTING_GUI);
		}
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		case TVN_SELCHANGED:
			{
				LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) lParam;

				switch (pnmtv->hdr.idFrom)
				{
				case IDC_SETTING_LIST:
					{
						if (setting_page)
							DestroyWindow(setting_page);

						HWND setting_list = pnmtv->hdr.hwndFrom;
						int id = pnmtv->itemNew.lParam;

						if (id >= 0 && id < ARRAY_COUNT(setting_pages))
						{
							setting_page = CreateDialog(GetModuleHandle(NULL),
								MAKEINTRESOURCE(setting_pages[id].dialog), hWnd, setting_pages[id].proc);

							RECT rect;
							GetClientRect(hWnd, &rect);
							rect.left = 100;
							MoveWindow(setting_page, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
							ShowWindow(setting_page, SW_SHOW);
						}
					}
					break;
				}
			}
			break;
		}
		break;

	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:
		setting_hwnd = NULL;
		setting_page = NULL;
		break;
	}

	return 0;
}

static void settings_show(int page = IDD_SETTING_AUDIO)
{
	HINSTANCE instance = GetModuleHandle(NULL);

	if (setting_hwnd == NULL)
	{
		setting_hwnd = CreateDialog(instance, MAKEINTRESOURCE(IDD_SETTINGS), NULL, settings_proc);
		ShowWindow(setting_hwnd, SW_SHOW);
	}
	else
	{
		SetForegroundWindow(setting_hwnd);
		SetActiveWindow(setting_hwnd);
	}

	for (int i = 0; i < ARRAY_COUNT(setting_pages); i++)
	{
		if (setting_pages[i].dialog == page)
		{
			TreeView_SelectItem(GetDlgItem(setting_hwnd, IDC_SETTING_LIST), setting_pages[i].item);
		}
	}
}


static INT_PTR CALLBACK song_info_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			song_info_t * info = song_get_info();
			SetDlgItemText(hWnd, IDC_SONG_TITLE, info->title);
			SetDlgItemText(hWnd, IDC_SONG_AUTHOR, info->author);
			SetDlgItemText(hWnd, IDC_SONG_COMMENT, info->comment);
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				song_info_t * info = song_get_info();
				GetDlgItemText(hWnd, IDC_SONG_TITLE, info->title, sizeof(info->title));
				GetDlgItemText(hWnd, IDC_SONG_AUTHOR, info->author, sizeof(info->author));
				GetDlgItemText(hWnd, IDC_SONG_COMMENT, info->comment, sizeof(info->comment));
				EndDialog(hWnd, 1);
			}
			break;

		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;
		}
		break;

	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;
	}
	return 0;
}

// show song info
int gui_show_song_info()
{
	return DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SONG_INFO), gui_get_window(), song_info_proc);
}

// -----------------------------------------------------------------------------------------
// MAIN MENU
// -----------------------------------------------------------------------------------------
static HMENU menu_main = NULL;
static HMENU menu_record = NULL;
static HMENU menu_output = NULL;
static HMENU menu_instrument = NULL;
static HMENU menu_config = NULL;
static HMENU menu_about = NULL;
static HMENU menu_keymap = NULL;
static HMENU menu_midi_shift = NULL;
static HMENU menu_key_popup = NULL;
static HMENU menu_key_note = NULL;
static HMENU menu_key_note_shift = NULL;
static HMENU menu_key_channel = NULL;
static HMENU menu_key_control = NULL;
static HMENU menu_play_speed = NULL;
static HMENU menu_setting_group = NULL;

static byte selected_key = 0;
static byte preview_key = 0;

// menu id
enum MENU_ID
{
	MENU_ID_NONE,
	MENU_ID_VST_PLUGIN,
	MENU_ID_VST_PLUGIN_NONE,
	MENU_ID_VST_PLUGIN_BROWSE,
	MENU_ID_VST_PLUGIN_EDITOR,
	MENU_ID_CONFIG_DEVICE,
	MENU_ID_KEY_MAP,
	MENU_ID_KEY_MAP_LOAD,
	MENU_ID_KEY_MAP_SAVE,
	MENU_ID_MIDI_SHIFT,
	MENU_ID_KEY_NOTE,
	MENU_ID_KEY_NOTE_SHIFT,
	MENU_ID_KEY_NOTE_CHANNEL,
	MENU_ID_KEY_CONTROL,
	MENU_ID_KEY_CLEAR,
	MENU_ID_KEY_CONFIG,
	MENU_ID_HELP_HOMEPAGE,
	MENU_ID_HELP_ABOUT,

	MENU_ID_FILE_OPEN,
	MENU_ID_FILE_SAVE,
	MENU_ID_FILE_RECORD,
	MENU_ID_FILE_PLAY,
	MENU_ID_FILE_STOP,

	MENU_ID_PLAY_SPEED,
	MENU_ID_SETTING_GROUP,
	MENU_ID_SETTING_GROUP_ADD,
	MENU_ID_SETTING_GROUP_DEL,
	MENU_ID_SETTING_GROUP_COPY,
	MENU_ID_SETTING_GROUP_PASTE,
	MENU_ID_SETTING_GROUP_DEFAULT,
	MENU_ID_SETTING_GROUP_CLEAR,
};

// midi shift menus
static struct midi_shift_t 
{
	const char * name;
	int shift;
}
midi_shift_items[] =
{
	{ "C	(0)",	0 },
	{ "bD	(+1)",	1 },
	{ "D	(+2)",	2 },
	{ "bE	(+3)",	3 },
	{ "E	(+4)",	4 },
	{ "F	(+5)",	5 },
	{ "#F	(+6)",	6 },
	{ "G	(+7)",	7 },
	{ "bA	(-4)",	-4 },
	{ "A	(-3)",	-3 },
	{ "bB	(-2)",	-2 },
	{ "B	(-1)",	-1 },
};

static struct key_control_t
{
	const char * name;
	const char * label;
	const char * keydown;
	const char * keyup;
}
key_controls[] = 
{
	{ "播放",				"Play",		"Play" },
	{ "录制",				"Rec",		"Record" },
	{ "停止",				"Stop",		"Stop" },
	{ "曲调+",				"Key+",		"KeySignature Inc 1" },
	{ "曲调-",				"Key-",		"KeySignature Dec 1" },
	{ "左手八度+",			"LO+",		"Octshift 0 Inc 1" },
	{ "左手八度-",			"LO-",		"Octshift 0 Dec 1" },
	{ "右手八度+",			"RO+",		"Octshift 1 Inc 1" },
	{ "右手八度-",			"R0-",		"Octshift 1 Dec 1" },
	{ "分组+",				"GP+",		"Group Inc 1" },
	{ "分组-",				"GP-",		"Group Dec 1" },
	{ "分组0",				"GP0",		"Group Set 0" },
	{ "分组1",				"GP1",		"Group Set 1" },
	{ "分组2",				"GP2",		"Group Set 2" },
	{ "延音踏板（开）",		"RSP+",		"Controller 0 SustainPedal 127" },
	{ "延音踏板（关）",		"RSP-",		"Controller 0 SustainPedal 0" },
	{ "延音踏板（翻转）",	"RSP",		"Controller 0 SustainPedal 0 Flip" },
	{ "音色+",			"P+",			"Program 0 1 Inc" },
	{ "音色-",			"P-",			"Program 0 1 Dec" },
	{ "音色0",			"P0",			"Program 0 0" },
	{ "音色1",			"P1",			"Program 0 1" },
	{ "音色2",			"P2",			"Program 0 2" },
};


// init menu
static int menu_init()
{
	// create main menu
	menu_main = CreateMenu();

	// create submenus
	menu_output = CreatePopupMenu();
	menu_instrument = CreatePopupMenu();
	menu_record = CreatePopupMenu();
	menu_config = CreatePopupMenu();
	menu_about = CreatePopupMenu();
	menu_keymap = CreatePopupMenu();
	menu_midi_shift = CreatePopupMenu();
	menu_key_popup = CreatePopupMenu();
	menu_key_note = CreatePopupMenu();
	menu_key_note_shift = CreatePopupMenu();
	menu_key_channel = CreatePopupMenu();
	menu_key_control = CreatePopupMenu();
	menu_play_speed = CreatePopupMenu();
	menu_setting_group = CreatePopupMenu();

	MENUINFO menuinfo;
	menuinfo.cbSize = sizeof(MENUINFO);
	menuinfo.fMask = MIM_STYLE;
	menuinfo.dwStyle = MNS_NOTIFYBYPOS;

	SetMenuInfo(menu_main, &menuinfo);
	SetMenuInfo(menu_key_popup, &menuinfo);

	AppendMenu(menu_main, MF_POPUP, (UINT_PTR)menu_record, "录音");
	AppendMenu(menu_main, MF_POPUP, (UINT_PTR)menu_instrument, "音源");
	AppendMenu(menu_main, MF_POPUP, (UINT_PTR)menu_keymap, "键盘布局");
	AppendMenu(menu_main, MF_POPUP, (UINT_PTR)menu_setting_group, "键盘配置");
	AppendMenu(menu_main, MF_POPUP, (UINT_PTR)menu_config, "设置");
	AppendMenu(menu_main, MF_POPUP, (UINT_PTR)menu_about, "帮助");
	
	AppendMenu(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_OPEN, "打开...");
	AppendMenu(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_SAVE, "保存...");
	AppendMenu(menu_record, MF_SEPARATOR, 0, NULL);
	AppendMenu(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_RECORD, "录制");
	AppendMenu(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_PLAY, "播放");
	AppendMenu(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_STOP, "停止");

	AppendMenu(menu_config, MF_STRING, (UINT_PTR)MENU_ID_CONFIG_DEVICE, "选项...");
	AppendMenu(menu_config, MF_POPUP,  (UINT_PTR)menu_midi_shift, "曲调");
	AppendMenu(menu_config, MF_POPUP, (UINT_PTR)menu_play_speed, "播放速度");

	AppendMenu(menu_key_popup,	MF_POPUP, (UINT_PTR)menu_key_note, "音高");
	AppendMenu(menu_key_popup,	MF_POPUP, (UINT_PTR)menu_key_note_shift, "八度音高");
	AppendMenu(menu_key_popup,	MF_POPUP, (UINT_PTR)menu_key_channel, "通道");
	AppendMenu(menu_key_popup,	MF_POPUP, (UINT_PTR)menu_key_control, "控制器");
	AppendMenu(menu_key_popup,	MF_POPUP, (UINT_PTR)MENU_ID_KEY_CLEAR, "清除");
	AppendMenu(menu_key_popup,	MF_SEPARATOR, 0, NULL);
	AppendMenu(menu_key_popup,	MF_STRING, (UINT_PTR)MENU_ID_KEY_CONFIG, "布局配置...");


	AppendMenu(menu_about, MF_STRING, (UINT_PTR)MENU_ID_HELP_HOMEPAGE, "软件主页");
	AppendMenu(menu_about, MF_SEPARATOR, 0, NULL);
	AppendMenu(menu_about, MF_STRING, (UINT_PTR)MENU_ID_HELP_ABOUT, "关于...");

	return 0;
}

// shutdown menu
void menu_shutdown()
{
	DestroyMenu(menu_main);
	DestroyMenu(menu_record);
	DestroyMenu(menu_output);
	DestroyMenu(menu_instrument);
	DestroyMenu(menu_config);
	DestroyMenu(menu_about);
}

static INT_PTR CALLBACK about_dialog_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;
	}

	return 0;
}

// process menu message
int menu_on_command(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DWORD pos = wParam;
	HMENU menu = (HMENU)lParam;
	uint id = GetMenuItemID(menu, pos);
	char buff[256];

	switch (id)
	{
	case MENU_ID_VST_PLUGIN_BROWSE:
		{
			char temp[260];
			OPENFILENAME ofn;
			memset(&ofn, 0, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hwnd;
			ofn.lpstrFile = temp;
			ofn.lpstrFile[0] = 0;
			ofn.nMaxFile = sizeof(temp);
			ofn.lpstrFilter = "VST Plugins (*.dll)\0*.dll\0";
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

			if (GetOpenFileName(&ofn))
			{
				if (int err = config_select_instrument(INSTRUMENT_TYPE_VSTI, ofn.lpstrFile))
				{
					char buff[256];
					_snprintf(buff, sizeof(buff), "无法载入VST插件， 错误码：%d", err);
					MessageBoxA(gui_get_window(), buff, APP_NAME, MB_OK);
				}
			}
		}
		break;

	case MENU_ID_VST_PLUGIN_EDITOR:
		vsti_show_editor(!vsti_is_show_editor());
		break;

	case MENU_ID_VST_PLUGIN_NONE:
		config_select_instrument(INSTRUMENT_TYPE_VSTI, "");
		break;

	case MENU_ID_VST_PLUGIN:
		if (GetMenuString(menu, pos, buff, sizeof(buff), MF_BYPOSITION))
		{
			if (int err = config_select_instrument(INSTRUMENT_TYPE_VSTI, buff))
			{
				char buff[256];
				_snprintf(buff, sizeof(buff), "无法载入VST插件， 错误码：%d", err);
				MessageBoxA(gui_get_window(), buff, APP_NAME, MB_OK);
			}
		}
		break;

	case MENU_ID_CONFIG_DEVICE:
		settings_show();
		break;

	case MENU_ID_KEY_MAP:
		if (GetMenuString(menu, pos, buff, sizeof(buff), MF_BYPOSITION))
		{
			if (int err = config_set_keymap(buff))
			{
				char buff[256];
				_snprintf(buff, sizeof(buff), "无法载入按键映射， 错误码：%d", err);
				MessageBoxA(gui_get_window(), buff, APP_NAME, MB_OK);
			}
		}
		break;

	case MENU_ID_MIDI_SHIFT:
		if (pos >= 0 && pos < ARRAY_COUNT(midi_shift_items))
		{
			config_set_key_signature(midi_shift_items[pos].shift);
		}
		break;

	case MENU_ID_KEY_NOTE:
		{
			key_bind_t keydown;
			config_get_key_bind(selected_key, &keydown, NULL);

			if ((keydown.a >> 4) == 0x9)
			{
				keydown.b = (byte)((keydown.b / 12) * 12 + pos);
			}
			else
			{
				keydown.a = 0x90;
				keydown.b = (byte)(60 + pos);
				keydown.c = 127;
			}

			config_set_key_label(selected_key, NULL);
			config_set_key_bind(selected_key, &keydown, NULL);
		}
		break;

	case MENU_ID_KEY_NOTE_SHIFT:
		{
			key_bind_t keydown;
			config_get_key_bind(selected_key, &keydown, NULL);

			if ((keydown.a >> 4) == 0x9)
			{
				keydown.b = (byte)(12 + pos * 12 + (keydown.b % 12));
			}
			else
			{
				keydown.a = 0x90;
				keydown.b = (byte)(12 + pos * 12);
				keydown.c = 127;
			}

			config_set_key_label(selected_key, NULL);
			config_set_key_bind(selected_key, &keydown, NULL);
		}
		break;

	case MENU_ID_KEY_NOTE_CHANNEL:
		{
			key_bind_t keydown;
			config_get_key_bind(selected_key, &keydown, NULL);

			switch (keydown.a >> 4)
			{
			case 0x9:
			case 0x8:
			case 0xa:
			case 0xb:
			case 0xc:
			case 0xd:
				keydown.a = static_cast<byte>((keydown.a & 0xf0) | (pos & 0x0f));
				break;
			}

			config_set_key_bind(selected_key, &keydown, NULL);
		}
		break;

	case MENU_ID_KEY_CONTROL:
		if (pos >= 0 && pos < ARRAY_COUNT(key_controls))
		{
			char temp[256];
			if (key_controls[pos].keydown)
			{
				_snprintf(temp, sizeof(temp), "keydown %s %s", config_get_key_name(selected_key), key_controls[pos].keydown);
				config_parse_keymap(temp);
			}
			if (key_controls[pos].keyup)
			{
				_snprintf(temp, sizeof(temp), "keyup %s %s", config_get_key_name(selected_key), key_controls[pos].keyup);
				config_parse_keymap(temp);
			}
			if (key_controls[pos].label)
			{
				config_set_key_label(selected_key, key_controls[pos].label);
			}
		}
		break;

	case MENU_ID_KEY_CLEAR:
		{
			key_bind_t events[2];
			config_set_key_bind(selected_key, events + 0, events + 1);
			config_set_key_label(selected_key, NULL);
		}
		break;

	case MENU_ID_KEY_CONFIG:
		{
			settings_show(IDD_SETTING_KEYMAP);
		}
		break;

	case MENU_ID_HELP_HOMEPAGE:
		ShellExecute(NULL, "open", "http://freepiano.tiwb.com", NULL, NULL, 0);
		break;

	case MENU_ID_HELP_ABOUT:
		DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT), hwnd, about_dialog_proc);
		break;

	case MENU_ID_FILE_OPEN:
		{
			char temp[260];
			OPENFILENAME ofn;
			memset(&ofn, 0, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hwnd;
			ofn.lpstrFile = temp;
			ofn.lpstrFile[0] = 0;
			ofn.nMaxFile = sizeof(temp);
			ofn.lpstrFilter = "所有乐谱 (*.fpm, *.lyt)\0*.fpm;*.lyt\0FreePiano乐谱 (*.fpm)\0*.fpm\0iDreamPiano乐谱 (*.lyt)\0*.lyt\0";
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

			song_close();

			if (GetOpenFileName(&ofn))
			{
				int result = -1;
				const char * extension = PathFindExtension(ofn.lpstrFile);

				if (strcmp(extension, ".lyt") == 0)
					result = song_open_lyt(ofn.lpstrFile);

				else if (strcmp(extension, ".fpm") == 0)
					result = song_open(ofn.lpstrFile);

				if (result == 0)
					song_start_playback();
			}
		}
		break;

	case MENU_ID_FILE_SAVE:
		{
			int result = gui_show_song_info();

			if (result)
			{
				char temp[260];
				OPENFILENAME ofn;
				memset(&ofn, 0, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFile = temp;
				ofn.lpstrFile[0] = 0;
				ofn.nMaxFile = sizeof(temp);
				ofn.lpstrFilter = "FreePiano乐谱 (*.fpm)\0*.fpm\0";
				ofn.nFilterIndex = 1;
				ofn.Flags = OFN_PATHMUSTEXIST;
				ofn.lpstrDefExt = ".fpm";

				if (GetSaveFileName(&ofn))
				{
					PathRenameExtension(ofn.lpstrFile, ".fpm");
					int result = song_save(ofn.lpstrFile);

					if (result != 0)
					{
						MessageBox(gui_get_window(), "保存乐谱失败！", APP_NAME, MB_OK);
					}
				}
			}
		}
		break;

	case MENU_ID_FILE_RECORD:
		song_start_record();
		break;

	case MENU_ID_FILE_PLAY:
		song_start_playback();
		break;

	case MENU_ID_FILE_STOP:
		song_stop_playback();
		song_stop_record();
		break;

	case MENU_ID_PLAY_SPEED:
		if (GetMenuString(menu, pos, buff, sizeof(buff), MF_BYPOSITION))
		{
			double speed = atof(buff);
			if (speed <= 0) speed = 1;
			song_set_play_speed(speed);
		}
		break;

	case MENU_ID_KEY_MAP_LOAD:
		{
			char dir[260];
			char temp[260];

			config_get_media_path(dir, sizeof(dir), config_get_keymap());
			PathRemoveFileSpec(dir);

			OPENFILENAME ofn;
			memset(&ofn, 0, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hwnd;
			ofn.lpstrFile = temp;
			ofn.lpstrFile[0] = 0;
			ofn.nMaxFile = sizeof(temp);
			ofn.lpstrFilter = "FreePiano键盘配置 (*.map)\0*.map\0";
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_PATHMUSTEXIST;
			ofn.lpstrDefExt = ".map";
			ofn.nFileExtension = 4;
			ofn.lpstrInitialDir = dir;

			if (GetOpenFileName(&ofn))
				config_set_keymap(ofn.lpstrFile);
		}
		break;

	case MENU_ID_KEY_MAP_SAVE:
		{
			char dir[260];
			char temp[260] = {0};

			config_get_media_path(dir, sizeof(dir), config_get_keymap());
			PathRemoveFileSpec(dir);

			OPENFILENAME ofn;
			memset(&ofn, 0, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hwnd;
			ofn.lpstrFile = temp;
			ofn.lpstrFile[0] = 0;
			ofn.nMaxFile = sizeof(temp);
			ofn.lpstrFilter = "FreePiano键盘配置 (*.map)\0*.map\0";
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_PATHMUSTEXIST;
			ofn.lpstrDefExt = ".map";
			ofn.nFileExtension = 4;
			ofn.lpstrInitialDir = dir;

			if (GetSaveFileName(&ofn))
			{
				char buffer[40960];
				config_save_keymap(buffer, sizeof(buffer));

				FILE * fp = fopen(temp, "wb");
				if (fp)
				{
					fputs(buffer, fp);
					fclose(fp);

					config_set_keymap(temp);
				}
			}
		}
		break;
		
	case MENU_ID_SETTING_GROUP:
		if (GetMenuString(menu, pos, buff, sizeof(buff), MF_BYPOSITION))
			song_send_event(SM_SETTING_GROUP, 0, atoi(buff), 0, true);
		break;

	case MENU_ID_SETTING_GROUP_ADD:
		config_set_setting_group_count(config_get_setting_group_count() + 1);
		config_set_setting_group(config_get_setting_group_count() - 1);
		break;

	case MENU_ID_SETTING_GROUP_DEL:
		config_set_setting_group_count(config_get_setting_group_count() - 1);
		break;

	case MENU_ID_SETTING_GROUP_COPY:
		config_copy_key_setting();
		break;

	case MENU_ID_SETTING_GROUP_PASTE:
		config_paste_key_setting();
		break;

	case MENU_ID_SETTING_GROUP_DEFAULT:
		config_default_key_setting();
		break;

	case MENU_ID_SETTING_GROUP_CLEAR:
		break;
	};

	return 0;
}

int menu_on_popup(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HMENU menu = (HMENU)wParam;

	if (uMsg == WM_INITMENUPOPUP)
	{
		if (menu == menu_instrument)
		{
			// remove all menu items.
			while (int count = GetMenuItemCount(menu))
				RemoveMenu(menu, count - 1, MF_BYPOSITION);

			struct callback : vsti_enum_callback
			{
				void operator ()(const char * value)
				{
					const char * start = PathFindFileName(value);

					if (start)
					{
						char buffer[256];
						strncpy(buffer, start, sizeof(buffer));
						PathRemoveExtension(buffer);

						if (!found && _stricmp(buffer, config_get_instrument_path()) == 0)
						{
							found = true;
							AppendMenu(menu_instrument, MF_STRING | MF_CHECKED, 0, buffer);
						}
						else
						{
							AppendMenu(menu_instrument, MF_STRING, (UINT_PTR)(MENU_ID_VST_PLUGIN), buffer);
						}
					}
				}

				bool found;
			};

			callback cb;
			cb.found = *config_get_instrument_path() == 0;

			AppendMenu(menu_instrument, MF_STRING, (UINT_PTR)MENU_ID_VST_PLUGIN_BROWSE, "浏览...");
			AppendMenu(menu_instrument, MF_STRING | (vsti_is_show_editor() ? MF_CHECKED : 0) , (UINT_PTR)MENU_ID_VST_PLUGIN_EDITOR, "显示音源界面");
			AppendMenu(menu_instrument, MF_SEPARATOR, 0, NULL);
			AppendMenu(menu_instrument, MF_STRING | (cb.found ? MF_CHECKED : 0), (UINT_PTR)MENU_ID_VST_PLUGIN_NONE, "（无）");

			vsti_enum_plugins(cb);

			if (!cb.found)
				AppendMenu(menu_instrument, MF_STRING | MF_CHECKED, 0, config_get_instrument_path());
		}

		// keyboard map
		else if (menu == menu_keymap)
		{
			struct enum_callback : keymap_enum_callback
			{
				void operator ()(const char * value)
				{
					const char * filename = PathFindFileName(value);
					bool checked = _stricmp(filename, config_get_keymap()) == 0;
					AppendMenu(menu, MF_STRING | (checked ? MF_CHECKED : 0), MENU_ID_KEY_MAP, filename);
					found |= checked;
				};

				HMENU menu;
				bool found;
			};

			// remove all menu items.
			while (int count = GetMenuItemCount(menu))
				RemoveMenu(menu, count - 1, MF_BYPOSITION);

			// append action menus
			AppendMenu(menu, MF_STRING, MENU_ID_KEY_MAP_LOAD,	"载入...");
			AppendMenu(menu, MF_STRING, MENU_ID_KEY_MAP_SAVE,	"保存...");
			AppendMenu(menu, MF_SEPARATOR, 0, NULL);

			// enum key maps.
			enum_callback cb;
			cb.menu = menu;
			cb.found = !*config_get_keymap();
			keyboard_enum_keymap(cb);

			if (!cb.found)
				AppendMenu(menu, MF_STRING | MF_CHECKED, MENU_ID_KEY_MAP, config_get_keymap());
		}

		// midi shift
		else if (menu == menu_midi_shift)
		{
			// remove all menu items.
			while (int count = GetMenuItemCount(menu))
				RemoveMenu(menu, count - 1, MF_BYPOSITION);

			for (int i = 0; i < ARRAY_COUNT(midi_shift_items); i++)
			{
				bool checked = config_get_key_signature() == midi_shift_items[i].shift;
				AppendMenu(menu, MF_STRING | (checked ? MF_CHECKED : 0), MENU_ID_MIDI_SHIFT, midi_shift_items[i].name);
			}
		}

		else if (menu == menu_key_note)
		{
			static const char * keys[] = { "1", "#1", "2", "#2", "3", "4", "#4", "5", "#5", "6", "#6", "7" };

			// remove all menu items.
			while (int count = GetMenuItemCount(menu))
				RemoveMenu(menu, count - 1, MF_BYPOSITION);

			key_bind_t keydown;
			config_get_key_bind(selected_key, &keydown, NULL);

			int selected = -1;

			switch (keydown.a >> 4)
			{
			case 0x8: case 0x9: selected = keydown.b % 12;
			}

			for (int i = 0; i < ARRAY_COUNT(keys); i++)
			{
				AppendMenu(menu, MF_STRING | (selected == i ? MF_CHECKED : 0) , MENU_ID_KEY_NOTE, keys[i]);
			}
		}
		else if (menu == menu_key_note_shift)
		{
			static const char * shifts[] = {"-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4"};

			// remove all menu items.
			while (int count = GetMenuItemCount(menu))
				RemoveMenu(menu, count - 1, MF_BYPOSITION);

			key_bind_t keydown;
			config_get_key_bind(selected_key, &keydown, NULL);

			int selected = -1;

			switch (keydown.a >> 4)
			{
			case 0x8: case 0x9: selected = keydown.b / 12 - 1;
			}

			for (int i = 0; i < ARRAY_COUNT(shifts); i++)
				AppendMenu(menu, MF_STRING | (selected == i ? MF_CHECKED : 0), MENU_ID_KEY_NOTE_SHIFT, shifts[i]);
		}

		else if (menu == menu_key_channel)
		{
			static const char * channels[] = { "左手", "右手", };
			
			key_bind_t keydown;
			config_get_key_bind(selected_key, &keydown, NULL);
			int selected = (keydown.a & 0xf);

			// remove all menu items.
			while (int count = GetMenuItemCount(menu))
				RemoveMenu(menu, count - 1, MF_BYPOSITION);

			for (int i = 0; i < ARRAY_COUNT(channels); i++)
				AppendMenu(menu, MF_STRING | (selected == i ? MF_CHECKED : 0), MENU_ID_KEY_NOTE_CHANNEL, channels[i]);
		}

		else if (menu == menu_key_control)
		{
			// remove all menu items.
			while (int count = GetMenuItemCount(menu))
				RemoveMenu(menu, count - 1, MF_BYPOSITION);

			for (int i = 0; i < ARRAY_COUNT(key_controls); i++)
				AppendMenu(menu, MF_STRING, MENU_ID_KEY_CONTROL, key_controls[i].name);
		}

		else if (menu == menu_record)
		{
			EnableMenuItem(menu, MENU_ID_FILE_SAVE, MF_BYCOMMAND | (song_allow_save() ? MF_ENABLED : MF_DISABLED));
			EnableMenuItem(menu, MENU_ID_FILE_PLAY, MF_BYCOMMAND | (!song_is_empty() && !song_is_recording() && !song_is_playing() ? MF_ENABLED : MF_DISABLED));
			EnableMenuItem(menu, MENU_ID_FILE_STOP, MF_BYCOMMAND | (song_is_playing() || song_is_recording() ? MF_ENABLED : MF_DISABLED));
			EnableMenuItem(menu, MENU_ID_FILE_RECORD, MF_BYCOMMAND | (!song_is_recording() ? MF_ENABLED : MF_DISABLED));
		}

		else if (menu == menu_play_speed)
		{
			static const char * speeds[] = { "0.25x", "0.5x", "1.0x", "2.0x" };

			// remove all menu items.
			while (int count = GetMenuItemCount(menu))
				RemoveMenu(menu, count - 1, MF_BYPOSITION);

			for (int i = 0; i < ARRAY_COUNT(speeds); i++)
				AppendMenu(menu, MF_STRING | (song_get_play_speed() == atof(speeds[i]) ? MF_CHECKED : 0), MENU_ID_PLAY_SPEED, speeds[i]);
		}

		else if (menu == menu_setting_group)
		{
			// remove all menu items.
			while (int count = GetMenuItemCount(menu))
				RemoveMenu(menu, count - 1, MF_BYPOSITION);


			for (uint i = 0; i < config_get_setting_group_count(); i++)
			{
				char buff[256];
				_snprintf(buff, sizeof(buff), "%d", i);
				AppendMenu(menu, MF_STRING | (config_get_setting_group() == i ? MF_CHECKED : 0), MENU_ID_SETTING_GROUP, buff);
			}

			// group count
			AppendMenu(menu, MF_SEPARATOR, 0, NULL);
			AppendMenu(menu, MF_STRING, MENU_ID_SETTING_GROUP_ADD, "增加分组");
			AppendMenu(menu, MF_STRING, MENU_ID_SETTING_GROUP_DEL, "减少分组");
			AppendMenu(menu, MF_SEPARATOR, 0, NULL);
			AppendMenu(menu, MF_POPUP, MENU_ID_SETTING_GROUP_COPY, "拷贝配置");
			AppendMenu(menu, MF_POPUP, MENU_ID_SETTING_GROUP_PASTE, "粘贴配置");
			AppendMenu(menu, MF_POPUP, MENU_ID_SETTING_GROUP_DEFAULT, "恢复默认配置");
		}

	}
	else if (uMsg == WM_UNINITMENUPOPUP)
	{
	}

	return 0;
}

// popup key menu
void gui_popup_keymenu(byte code, int x, int y)
{
	preview_key = selected_key = code;
	TrackPopupMenuEx(menu_key_popup, TPM_LEFTALIGN, x, y, gui_get_window(), NULL);
	preview_key = -1;
}

// -----------------------------------------------------------------------------------------
// main window functions
// -----------------------------------------------------------------------------------------
static HWND mainhwnd = NULL;

static LRESULT CALLBACK windowproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool in_sizemove = false;

	switch (uMsg)
	{
	case WM_CREATE:
		DragAcceptFiles(hWnd, TRUE);
		break;

	case WM_ACTIVATE:
		keyboard_enable(WA_INACTIVE != LOWORD(wParam));
		break;

	case WM_SIZE:
		keyboard_enable(wParam != SIZE_MINIMIZED);
		break;

	case WM_SIZING:
		{
			if (!config_get_enable_resize_window())
			{
				RECT fixed_size = { 0, 0, default_client_width, default_client_height };
				RECT * rect = (RECT*)lParam;
				AdjustWindowRect(&fixed_size, GetWindowLong(hWnd, GWL_STYLE), GetMenu(hWnd) != NULL);

				switch (wParam)
				{
				case WMSZ_TOP:
				case WMSZ_TOPLEFT:
				case WMSZ_TOPRIGHT:
					rect->top = rect->bottom - (fixed_size.bottom - fixed_size.top);
					break;

				default:
					rect->bottom = rect->top + (fixed_size.bottom - fixed_size.top);
					break;
				}

				switch (wParam)
				{
				case WMSZ_LEFT:
				case WMSZ_TOPLEFT:
				case WMSZ_BOTTOMLEFT:
					rect->left = rect->right - (fixed_size.right - fixed_size.left);
					break;

				default:
					rect->right = rect->left + (fixed_size.right - fixed_size.left);
					break;
				}

				return 1;
			}
		};

	case WM_PAINT:
		display_render();
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_SYSCOMMAND:
		// disable sys menu
		if (wParam == SC_KEYMENU)
			return 0;
		break;

	case WM_ENTERMENULOOP:
		keyboard_enable(false);
		break;

	case WM_EXITMENULOOP:
		keyboard_enable(true);
		break;

	case WM_EXITSIZEMOVE:
		in_sizemove = false;
		break;

	case WM_TIMER:
		display_render();
		break;

	case WM_DESTROY:
		// quit application
		PostQuitMessage(0);
		break;

	case WM_MENUCOMMAND:
		return menu_on_command(hWnd, uMsg, wParam, lParam);

	case WM_INITMENUPOPUP:
	case WM_UNINITMENUPOPUP:
		return menu_on_popup(hWnd, uMsg, wParam, lParam);


	case WM_DROPFILES:
		{
			HDROP drop = (HDROP)wParam;
			char filepath[MAX_PATH];
			int len = DragQueryFile(drop, 0, filepath, sizeof(filepath));

			if (len > 0)
			{
				const char * extension = PathFindExtension(filepath);

				// drop a music file
				if (_stricmp(extension, ".lyt") == 0)
				{
					if (song_open_lyt(filepath) == 0)
						song_start_playback();
					return 0;
				}

				if (_stricmp(extension, ".fpm") == 0)
				{
					if (song_open(filepath) == 0)
						song_start_playback();
					return 0;
				}

				// drop a instrument
				if (_stricmp(extension, ".dll") == 0)
				{
					config_select_instrument(INSTRUMENT_TYPE_VSTI, filepath);
					return 0;
				}

				// drop a map file
				if (_stricmp(extension, ".map") == 0)
				{
					config_set_keymap(filepath);
					return 0;
				}
			}
		}
		break;
	}

	// display process message
	if (int ret = display_process_message(hWnd, uMsg, wParam, lParam))
		return ret;

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// init gui system
int gui_init()
{
	HINSTANCE hInstance = GetModuleHandle(NULL);

	// register window class
	WNDCLASSEX wc = { sizeof(wc), 0 };
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = &windowproc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_NORMAL));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "FreePianoMainWindow";
	wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_SMALL));

	RegisterClassEx(&wc);

	// init menu
	menu_init();
	
	uint style = WS_OVERLAPPEDWINDOW;

	int screenwidth = GetSystemMetrics(SM_CXSCREEN);
	int screenheight = GetSystemMetrics(SM_CYSCREEN);

	RECT rect;
	rect.left = (screenwidth - default_client_width) / 2;
	rect.top = (screenheight - default_client_height) / 2;
	rect.right = rect.left + default_client_width;
	rect.bottom = rect.top + default_client_height;

	AdjustWindowRect(&rect, style, TRUE);

	// create window
	mainhwnd = CreateWindow("FreePianoMainWindow", APP_NAME, style,
		rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
		NULL, menu_main, hInstance, NULL);

	if (mainhwnd == NULL)
	{
		fprintf(stderr, "failed to create window");
		return -1;
	}

	// disable ime
	ImmAssociateContext(gui_get_window(), NULL);

	// create timer
	SetTimer(mainhwnd, 0, 1, NULL);

	return 0;
}

// shutdown gui system
void gui_shutdown()
{
	menu_shutdown();

	if (mainhwnd)
	{
		DestroyWindow(mainhwnd);
		mainhwnd = NULL;
	}
}

// get main window handle
HWND gui_get_window()
{
	return mainhwnd;
}

// show gui
void gui_show()
{
	ShowWindow(mainhwnd, SW_SHOW);
	display_render();
}

// get selected key
int gui_get_selected_key()
{
	return preview_key;
}