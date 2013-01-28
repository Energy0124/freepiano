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
#include "language.h"
#include "export_mp4.h"
#include "../res/resource.h"

#pragma comment(lib, "Shlwapi.lib")

// enable vistual style.
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")



static void try_open_song(int err) {
  if (err == 0) {
    gui_show_song_info();
    song_start_playback();
    return;
  }

  switch (err) {
   case -1: MessageBox(gui_get_window(), lang_load_string(IDS_ERR_OPEN_SONG), APP_NAME, MB_OK); break;
   case -2: MessageBox(gui_get_window(), lang_load_string(IDS_ERR_OPEN_SONG_VERSION), APP_NAME, MB_OK); break;
   default: MessageBox(gui_get_window(), lang_load_string(IDS_ERR_OPEN_SONG_FORMAT), APP_NAME, MB_OK); break;
  }
}

// -----------------------------------------------------------------------------------------
// config window
// -----------------------------------------------------------------------------------------
static HWND setting_hwnd = NULL;
static HWND setting_tab = NULL;
static HWND setting_page = NULL;

static INT_PTR CALLBACK settings_midi_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  static HMENU menu_midi_input_channel_popup = NULL;
  static HMENU menu_midi_input_enable_popup = NULL;

  struct helpers {
    static void update_midi_input(HWND list, int id, const char *device) {
      char buff[256];
      midi_input_config_t config;
      config_get_midi_input_config(device, &config);

      ListView_SetItemText(list, id, 1, (LPSTR)lang_load_string(config.enable ? IDS_MIDI_INPUT_LIST_ENABLED : IDS_MIDI_INPUT_LIST_DISABLED));

      sprintf_s(buff, "%d", config.channel);
      ListView_SetItemText(list, id, 2, (LPSTR)buff);
    }

    static void refresh_midi_inputs(HWND hWnd) {
      HWND input_list = GetDlgItem(hWnd, IDC_MIDI_INPUT_LIST);

      struct enum_callback : midi_enum_callback {
        void operator () (const char *value) {
          LVITEM lvI = {0};

          lvI.pszText   = (char*)value;
          lvI.mask      = LVIF_TEXT | LVIF_STATE;
          lvI.stateMask = 0;
          lvI.iSubItem  = 0;
          lvI.state     = 0;
          lvI.iItem     = index;
          ListView_InsertItem(list, &lvI);

          update_midi_input(list, index, value);
          index ++;
        }

        HWND list;
        int index;
      };

      // clear content
      ListView_DeleteAllItems(input_list);

      // build input list
      enum_callback callback;
      callback.list = input_list;
      callback.index = 0;
      midi_enum_input(callback);

      /*
      int selected = ListView_GetNextItem(input_list, -1, LVNI_FOCUSED);
      ListView_SetItemState(input_list, -1, 0, LVIS_SELECTED); // deselect all items
      ListView_EnsureVisible(input_list, selected, FALSE);
      ListView_SetItemState(input_list, selected, LVIS_SELECTED ,LVIS_SELECTED); // select item
      ListView_SetItemState(input_list, selected, LVIS_FOCUSED ,LVIS_FOCUSED); // optional
      */
    }

    static HMENU create_popup_menu() {
      HMENU menu = CreatePopupMenu();

      MENUINFO menuinfo;
      menuinfo.cbSize = sizeof(MENUINFO);
      menuinfo.fMask = MIM_STYLE;
      menuinfo.dwStyle = MNS_NOTIFYBYPOS;
      SetMenuInfo(menu, &menuinfo);

      return menu;
    }

    static void show_popup_menu(HMENU menu, HWND hWnd) {
      POINT pos;
      GetCursorPos(&pos);
      TrackPopupMenuEx(menu, TPM_LEFTALIGN, pos.x, pos.y, hWnd, NULL);
    }
  };

  switch (uMsg) {
   case WM_INITDIALOG: {
     LVCOLUMN lvc = {0};
     HWND input_list = GetDlgItem(hWnd, IDC_MIDI_INPUT_LIST);

     lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
     lvc.fmt = LVCFMT_LEFT;

     // Device
     lvc.pszText = (LPSTR)lang_load_string(IDS_MIDI_INPUT_LIST_DEVICE);
     lvc.cx = 300;
     ListView_InsertColumn(input_list, 0, &lvc);

     // Enable.
     lvc.pszText = (LPSTR)lang_load_string(IDS_MIDI_INPUT_LIST_ENABLE);
     lvc.cx = 60;
     lvc.fmt = LVCFMT_CENTER;
     ListView_InsertColumn(input_list, 2, &lvc);

     // Channel.
     lvc.pszText = (LPSTR)lang_load_string(IDS_MIDI_INPUT_LIST_CHANNEL);
     lvc.cx = 60;
     ListView_InsertColumn(input_list, 2, &lvc);

     // Set extended style.
     ListView_SetExtendedListViewStyle(input_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

     helpers::refresh_midi_inputs(hWnd);
     break;
   }

   case WM_DEVICECHANGE: {
     helpers::refresh_midi_inputs(hWnd);
     break;
   }

   case WM_COMMAND:
     switch (LOWORD(wParam)) {
     case IDC_MIDI_INPUT_LIST:
       break;
     }
     break;

   case WM_MENUCOMMAND: {
     DWORD pos = wParam;
     HMENU menu = (HMENU)lParam;

     if (menu == menu_midi_input_channel_popup || menu == menu_midi_input_enable_popup) {
       HWND list = GetDlgItem(hWnd, IDC_MIDI_INPUT_LIST);
       int row = ListView_GetNextItem(list, -1, LVNI_FOCUSED);

       char device_name[256] = "";
       ListView_GetItemText(list, row, 0, device_name, sizeof(device_name));
       midi_input_config_t config;
       config_get_midi_input_config(device_name, &config);

       if (menu == menu_midi_input_channel_popup) {
         char buff[256];

         if (GetMenuString(menu, pos, buff, sizeof(buff), MF_BYPOSITION)) {
           config.channel = atoi(buff);
         }
       }
       else if (menu == menu_midi_input_enable_popup) {
         config.enable = pos == 0;
       }

       config_set_midi_input_config(device_name, config);
       midi_open_inputs();

       helpers::update_midi_input(list, row, device_name);
     }

     DestroyMenu(menu_midi_input_channel_popup);
     menu_midi_input_channel_popup = NULL;

     DestroyMenu(menu_midi_input_enable_popup);
     menu_midi_input_enable_popup = NULL;
   }
   break;

   case WM_NOTIFY:
     if (LOWORD(wParam) == IDC_MIDI_INPUT_LIST) {
       switch (((LPNMHDR)lParam)->code) {
         case NM_CLICK:
         case NM_DBLCLK: {
           LPNMITEMACTIVATE item = (LPNMITEMACTIVATE)lParam;
           int row = item->iItem;
           int column = item->iSubItem;

           if (column == 1) {
             menu_midi_input_enable_popup = helpers::create_popup_menu();

             AppendMenu(menu_midi_input_enable_popup,  MF_STRING, (UINT_PTR)0, lang_load_string(IDS_MIDI_INPUT_LIST_ENABLED));
             AppendMenu(menu_midi_input_enable_popup,  MF_STRING, (UINT_PTR)0, lang_load_string(IDS_MIDI_INPUT_LIST_DISABLED));

             helpers::show_popup_menu(menu_midi_input_enable_popup, hWnd);
           }

           else if (column == 2) {
             menu_midi_input_channel_popup = helpers::create_popup_menu();

             for (int i = 0; i < 16; i++) {
               char buff[256];
               sprintf_s(buff, "%d", i);
               AppendMenu(menu_midi_input_channel_popup,  MF_STRING, (UINT_PTR)0, buff);
             }

             helpers::show_popup_menu(menu_midi_input_channel_popup, hWnd);
           }
           break;
         }
       }
     };
     break;
  }

  return 0;
}

static INT_PTR CALLBACK settings_audio_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  static char output_delay_text_format[256];
  static char output_volume_text_format[256];

  static const char *output_types[] = {
    "NONE",
    "DirectSound",
    "WASAPI",
    "ASIO",
  };

  switch (uMsg) {
   case WM_INITDIALOG: {
     HWND output_list = GetDlgItem(hWnd, IDC_OUTPUT_LIST);

     // output device list
     struct callback : dsound_enum_callback, wasapi_enum_callback, asio_enum_callback {
       void operator () (const char *value) {
         char buff[256];

         _snprintf(buff, sizeof(buff), "%s: %s", output_types[type], value);
         ComboBox_AddString(list, buff);
         if (config_get_output_type() == type) {
           if (_stricmp(value, config_get_output_device()) == 0) {
             ComboBox_SetCurSel(list, ComboBox_GetCount(list) - 1);
             selected = true;
           }
         }

         count++;
       }

       void operator () (const char *value, void *device) {
         operator () (value);
       }

       callback(int type, HWND list)
         : type(type)
         , list(list)
         , selected(false)
         , count(0) {
       }

       ~callback() {
         if (config_get_output_type() == type && !selected) {
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
     ComboBox_AddString(output_list, lang_load_string(IDS_SETTING_AUDIO_AUTO));
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
                 (WPARAM) TRUE,                  // redraw flag
                 (LPARAM) MAKELONG(0, 80));      // min. & max. positions

     SendMessage(output_delay, TBM_SETPOS,
                 (WPARAM) TRUE,                      // redraw flag
                 (LPARAM) config_get_output_delay()); // min. & max. positions


     // output volume slider
     HWND output_volume = GetDlgItem(hWnd, IDC_OUTPUT_VOLUME);

     SendMessage(output_volume, TBM_SETRANGE,
                 (WPARAM) TRUE,                  // redraw flag
                 (LPARAM) MAKELONG(0, 200));     // min. & max. positions

     SendMessage(output_volume, TBM_SETPOS,
                 (WPARAM) TRUE,                      // redraw flag
                 (LPARAM) config_get_output_volume()); // min. & max. positions
   }
   break;

   case WM_COMMAND:
     switch (LOWORD(wParam)) {
      case IDC_OUTPUT_LIST:
        switch (HIWORD(wParam)) {
         case CBN_SELCHANGE: {
           int result = 1;
           char temp[256];
           GetDlgItemText(hWnd, IDC_OUTPUT_LIST, temp, sizeof(temp));

           for (int i = 0; i < ARRAY_COUNT(output_types); i++) {
             int len = strlen(output_types[i]);
             if (_strnicmp(output_types[i], temp, len) == 0) {
               result = config_select_output(i, temp + len + 2);
               break;
             }
           }

           if (result) {
             config_select_output(OUTPUT_TYPE_AUTO, "");
             ComboBox_SetCurSel(GetDlgItem(hWnd, IDC_OUTPUT_LIST), 0);
           }

         }
         break;
        }
        break;
     }
     break;

   case WM_HSCROLL: {
     HWND output_delay = GetDlgItem(hWnd, IDC_OUTPUT_DELAY);
     HWND output_volume = GetDlgItem(hWnd, IDC_OUTPUT_VOLUME);

     if (output_delay == (HWND)lParam) {
       int pos = SendMessage(output_delay, TBM_GETPOS, 0, 0);

       // update output delay
       config_set_output_delay(pos);

       char temp[256];
       _snprintf(temp, sizeof(temp), output_delay_text_format, config_get_output_delay());
       SetDlgItemText(hWnd, IDC_OUTPUT_DELAY_TEXT, temp);
     } else if (output_volume == (HWND)lParam) {
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

static LRESULT CALLBACK EditSubProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  switch (uMsg) {
   case WM_CHAR:
     if (wParam == VK_RETURN) {
       SetFocus(NULL);
       SetFocus(hWnd);
       return 0;
     }
     break;
  }

  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static INT_PTR CALLBACK settings_keymap_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  struct helpers {
    static void update_content(HWND edit) {
      // save keymap config
      char *buff = config_save_keymap();

      uint tabstops = 46;
      LockWindowUpdate(edit);
      int scroll = GetScrollPos(edit, SB_VERT);
      Edit_SetTabStops(edit, 1, &tabstops);
      Edit_SetText(edit, buff);
      Edit_Scroll(edit, scroll, 0);
      LockWindowUpdate(NULL);

      free(buff);
    }

    static void apply_content(HWND edit) {
      uint buff_size = Edit_GetTextLength(edit);
      char *buff = (char*)malloc(buff_size);

      Edit_GetText(edit, buff, buff_size);
      config_parse_keymap(buff);

      free(buff);
    }
  };

  switch (uMsg) {
   case WM_INITDIALOG: {
     HWND output_content = GetDlgItem(hWnd, IDC_MAP_CONTENT);

     helpers::update_content(output_content);
   }
   break;

   case WM_COMMAND:
     switch (LOWORD(wParam)) {
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

static INT_PTR CALLBACK settings_gui_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
   case WM_INITDIALOG: {
     CheckDlgButton(hWnd, IDC_GUI_ENABLE_RESIZE, config_get_enable_resize_window());
     CheckDlgButton(hWnd, IDC_GUI_ENABLE_HOTKEY, !config_get_enable_hotkey());
     CheckDlgButton(hWnd, IDC_GUI_DISPLAY_MIDI_OUTPUT, config_get_midi_display() == MIDI_DISPLAY_OUTPUT);
     CheckDlgButton(hWnd, IDC_GUI_INSTRUMENT_SHOW_MIDI, config_get_instrument_show_midi());
     CheckDlgButton(hWnd, IDC_GUI_INSTRUMENT_SHOW_VSTI, config_get_instrument_show_vsti());
   }
   break;

   case WM_COMMAND:
     switch (LOWORD(wParam)) {
      case IDC_GUI_ENABLE_RESIZE:
        config_set_enable_resize_window(IsDlgButtonChecked(hWnd, IDC_GUI_ENABLE_RESIZE) != 0);
        break;

      case IDC_GUI_ENABLE_HOTKEY:
        config_set_enable_hotkey(!IsDlgButtonChecked(hWnd, IDC_GUI_ENABLE_HOTKEY));
        break;

      case IDC_GUI_DISPLAY_MIDI_OUTPUT:
        config_set_midi_display(IsDlgButtonChecked(hWnd, IDC_GUI_DISPLAY_MIDI_OUTPUT) ? 
          MIDI_DISPLAY_OUTPUT : MIDI_DISPLAY_INPUT);
        break;

      case IDC_GUI_INSTRUMENT_SHOW_MIDI:
       config_set_instrument_show_midi(0 != IsDlgButtonChecked(hWnd, IDC_GUI_INSTRUMENT_SHOW_MIDI));
       break;

      case IDC_GUI_INSTRUMENT_SHOW_VSTI:
       config_set_instrument_show_vsti(0 != IsDlgButtonChecked(hWnd, IDC_GUI_INSTRUMENT_SHOW_VSTI));
       break;
     }
     break;
  }

  return 0;
}

static struct setting_page_t {
  int dialog;
  DLGPROC proc;
  HTREEITEM item;
}
setting_pages[] = {
  { IDD_SETTING_AUDIO,    settings_audio_proc },
  { IDD_SETTING_MIDI,     settings_midi_proc },
  { IDD_SETTING_KEYMAP,   settings_keymap_proc },
  { IDD_SETTING_GUI,      settings_gui_proc },
};

static void add_setting_page(HWND list, const char *text, int page_id) {
  TVINSERTSTRUCT tvins;

  tvins.item.mask = TVIF_TEXT | TVIF_PARAM;
  tvins.item.pszText = (char *)text;
  tvins.item.cchTextMax = 0;

  for (int i = 0; i < ARRAY_COUNT(setting_pages); i++) {
    if (setting_pages[i].dialog == page_id) {
      tvins.item.lParam = i;
    }
  }

  tvins.hInsertAfter = NULL;
  tvins.hParent = TVI_ROOT;

  setting_pages[tvins.item.lParam].item = TreeView_InsertItem(list, &tvins);
}

static INT_PTR CALLBACK settings_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
   case WM_INITDIALOG: {
     HWND setting_list = GetDlgItem(hWnd, IDC_SETTING_LIST);
     add_setting_page(setting_list, lang_load_string(IDS_SETTING_LIST_AUDIO), IDD_SETTING_AUDIO);
     add_setting_page(setting_list, lang_load_string(IDS_SETTING_LIST_MIDI), IDD_SETTING_MIDI);
     add_setting_page(setting_list, lang_load_string(IDS_SETTING_LIST_KEYMAP), IDD_SETTING_KEYMAP);
     add_setting_page(setting_list, lang_load_string(IDS_SETTING_LIST_GUI), IDD_SETTING_GUI);
   }
   break;

   case WM_NOTIFY:
     switch (((LPNMHDR)lParam)->code) {
      case TVN_SELCHANGED: {
        LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) lParam;

        switch (pnmtv->hdr.idFrom) {
         case IDC_SETTING_LIST: {
           if (setting_page)
             DestroyWindow(setting_page);

           HWND setting_list = pnmtv->hdr.hwndFrom;
           int id = pnmtv->itemNew.lParam;

           if (id >= 0 && id < ARRAY_COUNT(setting_pages)) {
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

   case WM_DEVICECHANGE:
     if(setting_page != NULL) {
       SendMessage(setting_page, WM_DEVICECHANGE, wParam, lParam);
     }
     break;
  }

  return 0;
}

static void settings_show(int page = IDD_SETTING_AUDIO) {
  HINSTANCE instance = GetModuleHandle(NULL);

  if (setting_hwnd == NULL) {
    setting_hwnd = CreateDialog(instance, MAKEINTRESOURCE(IDD_SETTINGS), NULL, settings_proc);
    ShowWindow(setting_hwnd, SW_SHOW);
  } else {
    SetForegroundWindow(setting_hwnd);
    SetActiveWindow(setting_hwnd);
  }

  for (int i = 0; i < ARRAY_COUNT(setting_pages); i++) {
    if (setting_pages[i].dialog == page) {
      TreeView_SelectItem(GetDlgItem(setting_hwnd, IDC_SETTING_LIST), setting_pages[i].item);
    }
  }
}


static INT_PTR CALLBACK song_info_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
   case WM_INITDIALOG: {
     song_info_t *info = song_get_info();
     SetDlgItemText(hWnd, IDC_SONG_TITLE, info->title);
     SetDlgItemText(hWnd, IDC_SONG_AUTHOR, info->author);
     SetDlgItemText(hWnd, IDC_SONG_COMMENT, info->comment);
   }
   break;

   case WM_COMMAND:
     switch (LOWORD(wParam)) {
      case IDOK: {
        song_info_t *info = song_get_info();
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
int gui_show_song_info() {
  return DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SONG_INFO), gui_get_window(), song_info_proc);
}

// -----------------------------------------------------------------------------------------
// MAIN MENU
// -----------------------------------------------------------------------------------------
static byte selected_key = 0;
static byte preview_key = 0;

static HMENU menu_main = NULL;
static HMENU menu_record = NULL;
static HMENU menu_output = NULL;
static HMENU menu_instrument = NULL;
static HMENU menu_config = NULL;
static HMENU menu_about = NULL;
static HMENU menu_keymap = NULL;
static HMENU menu_key_popup = NULL;
static HMENU menu_key_note = NULL;
static HMENU menu_key_note_shift = NULL;
static HMENU menu_key_channel = NULL;
static HMENU menu_key_control = NULL;
static HMENU menu_play_speed = NULL;
static HMENU menu_setting_group = NULL;
static HMENU menu_export = NULL;
static HMENU menu_setting_group_change = NULL;

// menu id
enum MENU_ID {
  MENU_ID_NONE,
  MENU_ID_VST_PLUGIN,
  MENU_ID_INSTRUMENT_MIDI,
  MENU_ID_INSTRUMENT_VSTI_BROWSE,
  MENU_ID_INSTRUMENT_VSTI_EIDOTR,
  MENU_ID_CONFIG_OPTIONS,
  MENU_ID_KEY_MAP,
  MENU_ID_KEY_MAP_LOAD,
  MENU_ID_KEY_MAP_SAVE,
  MENU_ID_KEY_NOTE,
  MENU_ID_KEY_NOTE_SHIFT,
  MENU_ID_KEY_NOTE_CHANNEL,
  MENU_ID_KEY_CONTROL,
  MENU_ID_KEY_CLEAR,
  MENU_ID_KEY_CONFIG,
  MENU_ID_HELP_HOMEPAGE,
  MENU_ID_HELP_ONLINE,
  MENU_ID_HELP_ABOUT,

  MENU_ID_FILE_OPEN,
  MENU_ID_FILE_SAVE,
  MENU_ID_FILE_RECORD,
  MENU_ID_FILE_PLAY,
  MENU_ID_FILE_STOP,
  MENU_ID_FILE_EXPORT_MP4,

  MENU_ID_PLAY_SPEED,
  MENU_ID_SETTING_GROUP_CHANGE,
  MENU_ID_SETTING_GROUP_ADD,
  MENU_ID_SETTING_GROUP_DEL,
  MENU_ID_SETTING_GROUP_COPY,
  MENU_ID_SETTING_GROUP_PASTE,
  MENU_ID_SETTING_GROUP_DEFAULT,
  MENU_ID_SETTING_GROUP_CLEAR,

  MENU_ID_MIDI_INPUT_CHANNEL,
};

// init menu
static int menu_init() {
  // create main menu
  menu_main = CreateMenu();

  // create submenus
  menu_output = CreatePopupMenu();
  menu_instrument = CreatePopupMenu();
  menu_record = CreatePopupMenu();
  menu_config = CreatePopupMenu();
  menu_about = CreatePopupMenu();
  menu_keymap = CreatePopupMenu();
  menu_key_popup = CreatePopupMenu();
  menu_key_note = CreatePopupMenu();
  menu_key_note_shift = CreatePopupMenu();
  menu_key_channel = CreatePopupMenu();
  menu_key_control = CreatePopupMenu();
  menu_play_speed = CreatePopupMenu();
  menu_setting_group = CreatePopupMenu();
  menu_export = CreatePopupMenu();
  menu_setting_group_change = CreatePopupMenu();

  MENUINFO menuinfo;
  menuinfo.cbSize = sizeof(MENUINFO);
  menuinfo.fMask = MIM_STYLE;
  menuinfo.dwStyle = MNS_NOTIFYBYPOS;

  SetMenuInfo(menu_main, &menuinfo);
  SetMenuInfo(menu_key_popup, &menuinfo);

  // Main menu
  AppendMenu(menu_main, MF_POPUP, (UINT_PTR)menu_record, lang_load_string(IDS_MENU_FILE));
  AppendMenu(menu_main, MF_POPUP, (UINT_PTR)menu_instrument, lang_load_string(IDS_MENU_INSTRUMENT));
  AppendMenu(menu_main, MF_POPUP, (UINT_PTR)menu_keymap, lang_load_string(IDS_MENU_KEYMAP));
  AppendMenu(menu_main, MF_POPUP, (UINT_PTR)menu_setting_group, lang_load_string(IDS_MENU_KEYGROUP));
  AppendMenu(menu_main, MF_POPUP, (UINT_PTR)menu_config, lang_load_string(IDS_MENU_CONFIG));
  AppendMenu(menu_main, MF_POPUP, (UINT_PTR)menu_about, lang_load_string(IDS_MENU_HELP));

  // Record menu
  AppendMenu(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_OPEN, lang_load_string(IDS_MENU_FILE_OPEN));
  AppendMenu(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_SAVE, lang_load_string(IDS_MENU_FILE_SAVE));
  AppendMenu(menu_record, MF_POPUP, (UINT_PTR)menu_export, lang_load_string(IDS_MENU_FILE_EXPORT));
  AppendMenu(menu_record, MF_SEPARATOR, 0, NULL);
  AppendMenu(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_RECORD, lang_load_string(IDS_MENU_FILE_RECORD));
  AppendMenu(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_PLAY, lang_load_string(IDS_MENU_FILE_PLAY));
  AppendMenu(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_STOP, lang_load_string(IDS_MENU_FILE_STOP));

  AppendMenu(menu_export, MF_STRING, (UINT_PTR)MENU_ID_FILE_EXPORT_MP4, lang_load_string(IDS_MENU_FILE_EXPORT_MP4));

  // Config menu
  AppendMenu(menu_config, MF_STRING, (UINT_PTR)MENU_ID_CONFIG_OPTIONS, lang_load_string(IDS_MENU_CONFIG_OPTIONS));
  AppendMenu(menu_config, MF_POPUP, (UINT_PTR)menu_play_speed, lang_load_string(IDS_MENU_CONFIG_PLAYSPEED));

  // About menu
  AppendMenu(menu_about, MF_STRING, (UINT_PTR)MENU_ID_HELP_HOMEPAGE, lang_load_string(IDS_MENU_HELP_HOMEPAGE));
  AppendMenu(menu_about, MF_STRING, (UINT_PTR)MENU_ID_HELP_ONLINE, lang_load_string(IDS_MENU_HELP_ONLINE));
  AppendMenu(menu_about, MF_SEPARATOR, 0, NULL);
  AppendMenu(menu_about, MF_STRING, (UINT_PTR)MENU_ID_HELP_ABOUT, lang_load_string(IDS_MENU_HELP_ABOUT));

  // Setting group menu
  AppendMenu(menu_setting_group, MF_POPUP, (UINT_PTR)menu_setting_group_change, lang_load_string(IDS_MENU_SETTING_GROUP_CHANGE));
  AppendMenu(menu_setting_group, MF_SEPARATOR, 0, NULL);
  AppendMenu(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_ADD, lang_load_string(IDS_MENU_SETTING_GROUP_ADD));
  AppendMenu(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_DEL, lang_load_string(IDS_MENU_SETTING_GROUP_DEL));
  AppendMenu(menu_setting_group, MF_SEPARATOR, 0, NULL);
  AppendMenu(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_COPY, lang_load_string(IDS_MENU_SETTING_GROUP_COPY));
  AppendMenu(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_PASTE, lang_load_string(IDS_MENU_SETTING_GROUP_PASTE));
  AppendMenu(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_CLEAR, lang_load_string(IDS_MENU_SETTING_GROUP_CLEAR));
  AppendMenu(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_DEFAULT, lang_load_string(IDS_MENU_SETTING_GROUP_DEFAULT));


  // Popup menu
  AppendMenu(menu_key_popup,  MF_POPUP, (UINT_PTR)menu_key_note, lang_load_string(IDS_MENU_KEY_NOTE));
  AppendMenu(menu_key_popup,  MF_POPUP, (UINT_PTR)menu_key_note_shift, lang_load_string(IDS_MENU_KEY_NOTESHIFT));
  AppendMenu(menu_key_popup,  MF_POPUP, (UINT_PTR)menu_key_channel, lang_load_string(IDS_MENU_KEY_CHANNEL));
  AppendMenu(menu_key_popup,  MF_POPUP, (UINT_PTR)menu_key_control, lang_load_string(IDS_MENU_KEY_CONTROL));
  AppendMenu(menu_key_popup,  MF_POPUP, (UINT_PTR)MENU_ID_KEY_CLEAR, lang_load_string(IDS_MENU_KEY_CLEAR));
  AppendMenu(menu_key_popup,  MF_SEPARATOR, 0, NULL);
  AppendMenu(menu_key_popup,  MF_STRING, (UINT_PTR)MENU_ID_KEY_CONFIG, lang_load_string(IDS_MENU_KEY_SETTINGS));

  return 0;
}

// shutdown menu
static void menu_shutdown() {
  DestroyMenu(menu_main);
  DestroyMenu(menu_output);
  DestroyMenu(menu_instrument);
  DestroyMenu(menu_record);
  DestroyMenu(menu_config);
  DestroyMenu(menu_about);
  DestroyMenu(menu_keymap);
  DestroyMenu(menu_key_popup);
  DestroyMenu(menu_key_note);
  DestroyMenu(menu_key_note_shift);
  DestroyMenu(menu_key_channel);
  DestroyMenu(menu_key_control);
  DestroyMenu(menu_play_speed);
  DestroyMenu(menu_setting_group);
  DestroyMenu(menu_export);
}

static INT_PTR CALLBACK about_dialog_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
   case WM_CLOSE:
     EndDialog(hWnd, 0);
     break;
  }

  return 0;
}

// process menu message
int menu_on_command(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  DWORD pos = wParam;
  HMENU menu = (HMENU)lParam;
  uint id = GetMenuItemID(menu, pos);
  char buff[256];

  switch (id) {
   case MENU_ID_INSTRUMENT_VSTI_BROWSE: {
     char temp[260];
     OPENFILENAME ofn;
     memset(&ofn, 0, sizeof(ofn));
     ofn.lStructSize = sizeof(ofn);
     ofn.hwndOwner = hwnd;
     ofn.lpstrFile = temp;
     ofn.lpstrFile[0] = 0;
     ofn.nMaxFile = sizeof(temp);
     ofn.lpstrFilter = lang_load_filter_string(IDS_OPEN_FILTER_VST);
     ofn.nFilterIndex = 1;
     ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;


     if (GetOpenFileName(&ofn)) {
       if (int err = config_select_instrument(INSTRUMENT_TYPE_VSTI, ofn.lpstrFile)) {
         char buff[256];
         lang_format_string(buff, sizeof(buff), IDS_ERR_LOAD_VST, err);
         MessageBoxA(gui_get_window(), buff, APP_NAME, MB_OK);
       }
     }
   }
   break;

   case MENU_ID_INSTRUMENT_VSTI_EIDOTR:
     vsti_show_editor(!vsti_is_show_editor());
     break;

   case MENU_ID_INSTRUMENT_MIDI:
     if (GetMenuString(menu, pos, buff, sizeof(buff), MF_BYPOSITION)) {
       char *type_str = strchr(buff, '\t');
       if (type_str) *type_str = '\0';

       if (int err = config_select_instrument(INSTRUMENT_TYPE_MIDI, buff)) {
         char buff[256];
         lang_format_string(buff, sizeof(buff), IDS_ERR_LOAD_MIDI, err);
         MessageBoxA(gui_get_window(), buff, APP_NAME, MB_OK);
       }
     }
     break;

   case MENU_ID_VST_PLUGIN:
     if (GetMenuString(menu, pos, buff, sizeof(buff), MF_BYPOSITION)) {
       char *type_str = strchr(buff, '\t');
       if (type_str) *type_str = '\0';

       if (int err = config_select_instrument(INSTRUMENT_TYPE_VSTI, buff)) {
         char buff[256];
         lang_format_string(buff, sizeof(buff), IDS_ERR_LOAD_VST, err);
         MessageBoxA(gui_get_window(), buff, APP_NAME, MB_OK);
       }
     }
     break;

   case MENU_ID_CONFIG_OPTIONS:
     settings_show();
     break;

   case MENU_ID_KEY_MAP:
     if (GetMenuString(menu, pos, buff, sizeof(buff), MF_BYPOSITION)) {
       if (int err = config_set_keymap(buff)) {
         char buff[256];
         lang_format_string(buff, sizeof(buff), IDS_ERR_LOAD_KEYMAP, err);
         MessageBoxA(gui_get_window(), buff, APP_NAME, MB_OK);
       }
     }
     break;

   case MENU_ID_KEY_NOTE: {
     key_bind_t keydown;
     config_bind_get_keydown(selected_key, &keydown, 1);

     if ((keydown.a >> 4) == 0x9) {
       keydown.b = (byte)((keydown.b / 12) * 12 + pos);
     } else {
       keydown.a = 0x90;
       keydown.b = (byte)(60 + pos);
       keydown.c = 127;
     }

     config_bind_set_label(selected_key, NULL);
     config_bind_clear_keydown(selected_key);
     config_bind_clear_keyup(selected_key);
     config_bind_add_keydown(selected_key, keydown);
   }
   break;

   case MENU_ID_KEY_NOTE_SHIFT: {
     key_bind_t keydown;
     config_bind_get_keydown(selected_key, &keydown, 1);

     if ((keydown.a >> 4) == 0x9) {
       keydown.b = (byte)(12 + pos * 12 + (keydown.b % 12));
     } else {
       keydown.a = 0x90;
       keydown.b = (byte)(12 + pos * 12);
       keydown.c = 127;
     }

     config_bind_set_label(selected_key, NULL);
     config_bind_clear_keydown(selected_key);
     config_bind_clear_keyup(selected_key);
     config_bind_add_keydown(selected_key, keydown);
   }
   break;

   case MENU_ID_KEY_NOTE_CHANNEL: {
     key_bind_t keydown;
     config_bind_get_keydown(selected_key, &keydown, 1);

     switch (keydown.a >> 4) {
      case 0x9:
      case 0x8:
      case 0xa:
      case 0xb:
      case 0xc:
      case 0xd:
        keydown.a = static_cast<byte>((keydown.a & 0xf0) | (pos & 0x0f));
        break;
     }

     config_bind_clear_keydown(selected_key);
     config_bind_clear_keyup(selected_key);
     config_bind_add_keydown(selected_key, keydown);
   }
   break;

   case MENU_ID_KEY_CONTROL:
     if (lang_text_open(IDR_TEXT_CONTROLLERS)) {
       config_bind_clear_keydown(selected_key);
       config_bind_clear_keyup(selected_key);
       config_bind_set_label(selected_key, NULL);

       DWORD id = -1;
       char line[4096];
       while (lang_text_readline(line, sizeof(line))) {
         if (line[0] == '#') {
           if (++id > pos)
             break;
         } else if (id == pos) {
           char temp[4096];
           _snprintf(temp, sizeof(temp), line, config_get_key_name(selected_key));
           config_parse_keymap(temp);
         }
       }
       lang_text_close();
     }
     break;

   case MENU_ID_KEY_CLEAR: {
     config_bind_clear_keydown(selected_key);
     config_bind_clear_keyup(selected_key);
     config_bind_set_label(selected_key, NULL);
   }
   break;

   case MENU_ID_KEY_CONFIG: {
     settings_show(IDD_SETTING_KEYMAP);
   }
   break;

   case MENU_ID_HELP_HOMEPAGE:
     ShellExecute(NULL, "open", "http://freepiano.tiwb.com", NULL, NULL, 0);
     break;

   case MENU_ID_HELP_ONLINE:
     ShellExecute(NULL, "open", "http://freepiano.tiwb.com/category/help", NULL, NULL, 0);
     break;

   case MENU_ID_HELP_ABOUT:
     DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT), hwnd, about_dialog_proc);
     break;

   case MENU_ID_FILE_OPEN: {
     char temp[260];
     OPENFILENAME ofn;
     memset(&ofn, 0, sizeof(ofn));
     ofn.lStructSize = sizeof(ofn);
     ofn.hwndOwner = hwnd;
     ofn.lpstrFile = temp;
     ofn.lpstrFile[0] = 0;
     ofn.nMaxFile = sizeof(temp);
     ofn.lpstrFilter = lang_load_filter_string(IDS_OPEN_FILTER_SONG);
     ofn.nFilterIndex = 1;
     ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

     song_close();

     if (GetOpenFileName(&ofn)) {
       int result = -1;
       const char *extension = PathFindExtension(ofn.lpstrFile);

       if (strcmp(extension, ".lyt") == 0)
         try_open_song(song_open_lyt(ofn.lpstrFile));

       else if (strcmp(extension, ".fpm") == 0)
         try_open_song(song_open(ofn.lpstrFile));
     }
   }
   break;

   case MENU_ID_FILE_SAVE: {
     int result = gui_show_song_info();

     if (result) {
       char temp[260];
       OPENFILENAME ofn;
       memset(&ofn, 0, sizeof(ofn));
       ofn.lStructSize = sizeof(ofn);
       ofn.hwndOwner = hwnd;
       ofn.lpstrFile = temp;
       ofn.lpstrFile[0] = 0;
       ofn.nMaxFile = sizeof(temp);
       ofn.lpstrFilter = lang_load_filter_string(IDS_SAVE_FILTER_SONG);
       ofn.nFilterIndex = 1;
       ofn.Flags = OFN_PATHMUSTEXIST;
       ofn.lpstrDefExt = ".fpm";

       if (GetSaveFileName(&ofn)) {
         PathRenameExtension(ofn.lpstrFile, ".fpm");
         int result = song_save(ofn.lpstrFile);

         if (result != 0) {
           MessageBox(gui_get_window(), lang_load_string(IDS_ERR_SAVE_SONG), APP_NAME, MB_OK);
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
     if (GetMenuString(menu, pos, buff, sizeof(buff), MF_BYPOSITION)) {
       double speed = atof(buff);
       if (speed <= 0) speed = 1;
       song_set_play_speed(speed);
     }
     break;

   case MENU_ID_KEY_MAP_LOAD: {
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
     ofn.lpstrFilter = lang_load_filter_string(IDS_FILTER_MAP);
     ofn.nFilterIndex = 1;
     ofn.Flags = OFN_PATHMUSTEXIST;
     ofn.lpstrDefExt = ".map";
     ofn.nFileExtension = 4;
     ofn.lpstrInitialDir = dir;

     if (GetOpenFileName(&ofn))
       config_set_keymap(ofn.lpstrFile);
   }
   break;

   case MENU_ID_KEY_MAP_SAVE: {
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
     ofn.lpstrFilter = lang_load_filter_string(IDS_FILTER_MAP);
     ofn.nFilterIndex = 1;
     ofn.Flags = OFN_PATHMUSTEXIST;
     ofn.lpstrDefExt = ".map";
     ofn.nFileExtension = 4;
     ofn.lpstrInitialDir = dir;

     if (GetSaveFileName(&ofn)) {
       char *buffer = config_save_keymap();

       FILE *fp = fopen(temp, "wb");
       if (fp) {
         fputs(buffer, fp);
         fclose(fp);

         config_set_keymap(temp);
       }

       free(buffer);
     }
   }
   break;

   case MENU_ID_SETTING_GROUP_CHANGE:
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
     config_clear_key_setting();
     break;

   case MENU_ID_FILE_EXPORT_MP4: {
     song_stop_playback();

     char temp[260];
     OPENFILENAME ofn;
     memset(&ofn, 0, sizeof(ofn));
     ofn.lStructSize = sizeof(ofn);
     ofn.hwndOwner = hwnd;
     ofn.lpstrFile = temp;
     ofn.lpstrFile[0] = 0;
     ofn.nMaxFile = sizeof(temp);
     ofn.lpstrFilter = lang_load_filter_string(IDS_SAVE_FILTER_MP4);
     ofn.nFilterIndex = 1;
     ofn.Flags = OFN_PATHMUSTEXIST;
     ofn.lpstrDefExt = ".mp4";

     if (GetSaveFileName(&ofn)) {
       PathRenameExtension(ofn.lpstrFile, ".mp4");
       export_mp4(ofn.lpstrFile);
     }
   }
   break;
  }

  return 0;
}

int menu_on_popup(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  HMENU menu = (HMENU)wParam;

  if (uMsg == WM_INITMENUPOPUP) {
    if (menu == menu_instrument) {

      // enum midi devices
      struct enum_midi_callback : midi_enum_callback {
        void operator () (const char *value) {
          bool selected = false;
          char buffer[256];
          strncpy(buffer, value, sizeof(buffer));

          if (!found) {
            if (config_get_instrument_type() == INSTRUMENT_TYPE_MIDI &&
              _stricmp(buffer, config_get_instrument_path()) == 0) {
                selected = true;
                found = true;
            }
          }

          strcat_s(buffer, "\tMIDI");
          AppendMenu(menu_instrument, MF_STRING | (selected ? MF_CHECKED : 0), (UINT_PTR)(MENU_ID_INSTRUMENT_MIDI), buffer);
        }

        enum_midi_callback() {
          found = false;
        }

        bool found;
      };

      // enum vsti plugins.
      struct enum_vsti_callback : vsti_enum_callback {
        void operator () (const char *value) {
          bool selected = false;
          char buffer[256];

          if (only_filename) {
            const char *start = PathFindFileName(value);
            if (start) {
              strncpy(buffer, start, sizeof(buffer));
              PathRemoveExtension(buffer);
            } else {
              strcpy_s(buffer, value);
            }
          } else {
            strcpy_s(buffer, value);
          }

          if (!found) {
            if (config_get_instrument_type() == INSTRUMENT_TYPE_VSTI &&
              _stricmp(buffer, config_get_instrument_path()) == 0) {
                found = true;
                selected = true;
            }
          }

          strcat_s(buffer, "\tVSTi");
          AppendMenu(menu_instrument, MF_STRING | (selected ? MF_CHECKED : 0), (UINT_PTR)(MENU_ID_VST_PLUGIN), buffer);
        }

        enum_vsti_callback() {
          only_filename = true;
          found = false;
        }

        bool only_filename;
        bool found;
      };

      // remove all menu items.
      while (int count = GetMenuItemCount(menu))
        RemoveMenu(menu, count - 1, MF_BYPOSITION);

      // Load vsti plugin menu
      AppendMenu(menu_instrument, MF_STRING,
        (UINT_PTR)MENU_ID_INSTRUMENT_VSTI_BROWSE, lang_load_string(IDS_MENU_INSTRUMENT_BROWSE));

      AppendMenu(menu_instrument, MF_STRING | (vsti_is_show_editor() ? MF_CHECKED : 0),
        (UINT_PTR)MENU_ID_INSTRUMENT_VSTI_EIDOTR, lang_load_string(IDS_MENU_INSTRUMENT_GUI));

      AppendMenu(menu_instrument, MF_SEPARATOR, 0, NULL);

      enum_midi_callback midi_cb;
      if (config_get_instrument_show_midi()) {
        midi_enum_output(midi_cb);
      }

      if (!midi_cb.found && config_get_instrument_type() == INSTRUMENT_TYPE_MIDI) {
        midi_cb(config_get_instrument_path());
      }

      enum_vsti_callback vsti_cb;
      if (config_get_instrument_show_vsti()) {
        vsti_enum_plugins(vsti_cb);
      }

      if (!vsti_cb.found && config_get_instrument_type() == INSTRUMENT_TYPE_VSTI) {
        vsti_cb.only_filename = false;
        vsti_cb(config_get_instrument_path());
      }
    }
    // keyboard map
    else if (menu == menu_keymap) {
      struct enum_callback : keymap_enum_callback {
        void operator () (const char *value) {
          const char *filename = PathFindFileName(value);
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
      AppendMenu(menu, MF_STRING, MENU_ID_KEY_MAP_LOAD,   lang_load_string(IDS_MENU_KEYMAP_LOAD));
      AppendMenu(menu, MF_STRING, MENU_ID_KEY_MAP_SAVE,   lang_load_string(IDS_MENU_KEYMAP_SAVE));
      AppendMenu(menu, MF_SEPARATOR, 0, NULL);

      // enum key maps.
      enum_callback cb;
      cb.menu = menu;
      cb.found = !*config_get_keymap();
      keyboard_enum_keymap(cb);

      if (!cb.found)
        AppendMenu(menu, MF_STRING | MF_CHECKED, MENU_ID_KEY_MAP, config_get_keymap());
    }
    else if (menu == menu_key_note) {
      static const char *keys[] = { "1", "#1", "2", "#2", "3", "4", "#4", "5", "#5", "6", "#6", "7" };

      // remove all menu items.
      while (int count = GetMenuItemCount(menu))
        RemoveMenu(menu, count - 1, MF_BYPOSITION);

      key_bind_t keydown;
      config_bind_get_keydown(selected_key, &keydown, 1);

      int selected = -1;

      switch (keydown.a >> 4) {
       case 0x8: case 0x9: selected = keydown.b % 12;
      }

      for (int i = 0; i < ARRAY_COUNT(keys); i++) {
        AppendMenu(menu, MF_STRING | (selected == i ? MF_CHECKED : 0), MENU_ID_KEY_NOTE, keys[i]);
      }
    }
    else if (menu == menu_key_note_shift) {
      static const char *shifts[] = {"-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4"};

      // remove all menu items.
      while (int count = GetMenuItemCount(menu))
        RemoveMenu(menu, count - 1, MF_BYPOSITION);

      key_bind_t keydown;
      config_bind_get_keydown(selected_key, &keydown, 1);

      int selected = -1;

      switch (keydown.a >> 4) {
       case 0x8: case 0x9: selected = keydown.b / 12 - 1;
      }

      for (int i = 0; i < ARRAY_COUNT(shifts); i++)
        AppendMenu(menu, MF_STRING | (selected == i ? MF_CHECKED : 0), MENU_ID_KEY_NOTE_SHIFT, shifts[i]);
    }
    else if (menu == menu_key_channel) {
      const char * *channels = lang_load_string_array(IDS_MENU_KEY_CHANNELS);

      key_bind_t keydown;
      config_bind_get_keydown(selected_key, &keydown, 1);
      int selected = (keydown.a & 0xf);

      // remove all menu items.
      while (int count = GetMenuItemCount(menu))
        RemoveMenu(menu, count - 1, MF_BYPOSITION);

      for (int i = 0; channels[i]; i++)
        AppendMenu(menu, MF_STRING | (selected == i ? MF_CHECKED : 0), MENU_ID_KEY_NOTE_CHANNEL, channels[i]);
    }
    else if (menu == menu_key_control) {
      // remove all menu items.
      while (int count = GetMenuItemCount(menu))
        RemoveMenu(menu, count - 1, MF_BYPOSITION);

      if (lang_text_open(IDR_TEXT_CONTROLLERS)) {
        char line[4096];
        while (lang_text_readline(line, sizeof(line))) {
          if (line[0] == '#')
            AppendMenu(menu, MF_STRING, MENU_ID_KEY_CONTROL, line + 1);
        }
        lang_text_close();
      }
    }
    else if (menu == menu_record) {
      EnableMenuItem(menu, MENU_ID_FILE_SAVE, MF_BYCOMMAND | (song_allow_save() ? MF_ENABLED : MF_DISABLED));
      EnableMenuItem(menu, MENU_ID_FILE_PLAY, MF_BYCOMMAND | (!song_is_empty() && !song_is_recording() && !song_is_playing() ? MF_ENABLED : MF_DISABLED));
      EnableMenuItem(menu, MENU_ID_FILE_STOP, MF_BYCOMMAND | (song_is_playing() || song_is_recording() ? MF_ENABLED : MF_DISABLED));
      EnableMenuItem(menu, MENU_ID_FILE_RECORD, MF_BYCOMMAND | (!song_is_recording() ? MF_ENABLED : MF_DISABLED));

      bool enable_export = (config_get_instrument_type() == INSTRUMENT_TYPE_VSTI) && !song_is_empty();
      EnableMenuItem(menu, (UINT)menu_export, MF_BYCOMMAND | (enable_export ? MF_ENABLED : MF_DISABLED));
    }
    else if (menu == menu_play_speed) {
      static const char *speeds[] = { "0.25x", "0.5x", "1.0x", "2.0x" };

      // remove all menu items.
      while (int count = GetMenuItemCount(menu))
        RemoveMenu(menu, count - 1, MF_BYPOSITION);

      for (int i = 0; i < ARRAY_COUNT(speeds); i++)
        AppendMenu(menu, MF_STRING | (song_get_play_speed() == atof(speeds[i]) ? MF_CHECKED : 0), MENU_ID_PLAY_SPEED, speeds[i]);
    }
    else if (menu == menu_setting_group_change) {
      // remove all menu items.
      while (int count = GetMenuItemCount(menu))
        RemoveMenu(menu, count - 1, MF_BYPOSITION);

      uint group_count = config_get_setting_group_count();

      for (uint i = 0; i < group_count; i++) {
        char buff[256];
        _snprintf(buff, sizeof(buff), "%d", i);
        AppendMenu(menu, MF_STRING | (config_get_setting_group() == i ? MF_CHECKED : 0), MENU_ID_SETTING_GROUP_CHANGE, buff);
      }
    }

  } else if (uMsg == WM_UNINITMENUPOPUP) {
  }

  return 0;
}

// popup key menu
void gui_popup_keymenu(byte code, int x, int y) {
  preview_key = selected_key = code;
  TrackPopupMenuEx(menu_key_popup, TPM_LEFTALIGN, x, y, gui_get_window(), NULL);
  preview_key = -1;
}

// -----------------------------------------------------------------------------------------
// main window functions
// -----------------------------------------------------------------------------------------
static HWND mainhwnd = NULL;

static LRESULT CALLBACK windowproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  static bool in_sizemove = false;

  switch (uMsg) {
   case WM_CREATE:
     DragAcceptFiles(hWnd, TRUE);
     break;

   case WM_ACTIVATE:
     keyboard_enable(WA_INACTIVE != LOWORD(wParam));
     break;

   case WM_SIZE:
     keyboard_enable(wParam != SIZE_MINIMIZED);
     break;

   case WM_SIZING: {
     if (!config_get_enable_resize_window()) {
       RECT fixed_size = { 0, 0, display_get_width(), display_get_height() };
       RECT *rect = (RECT *)lParam;
       AdjustWindowRect(&fixed_size, GetWindowLong(hWnd, GWL_STYLE), GetMenu(hWnd) != NULL);

       switch (wParam) {
        case WMSZ_TOP:
        case WMSZ_TOPLEFT:
        case WMSZ_TOPRIGHT:
          rect->top = rect->bottom - (fixed_size.bottom - fixed_size.top);
          break;

        default:
          rect->bottom = rect->top + (fixed_size.bottom - fixed_size.top);
          break;
       }

       switch (wParam) {
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


   case WM_DROPFILES: {
     HDROP drop = (HDROP)wParam;
     char filepath[MAX_PATH];
     int len = DragQueryFile(drop, 0, filepath, sizeof(filepath));

     if (len > 0) {
       const char *extension = PathFindExtension(filepath);

       // drop a music file
       if (_stricmp(extension, ".lyt") == 0) {
         try_open_song(song_open_lyt(filepath));
         return 0;
       }

       if (_stricmp(extension, ".fpm") == 0) {
         try_open_song(song_open(filepath));
         return 0;
       }

       // drop a instrument
       if (_stricmp(extension, ".dll") == 0) {
         config_select_instrument(INSTRUMENT_TYPE_VSTI, filepath);
         return 0;
       }

       // drop a map file
       if (_stricmp(extension, ".map") == 0) {
         config_set_keymap(filepath);
         return 0;
       }
     }
   }
   break;

   case WM_DEVICECHANGE: {
     midi_open_inputs();
   }
   break;
  }

  // display process message
  if (int ret = display_process_message(hWnd, uMsg, wParam, lParam))
    return ret;

  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// init gui system
int gui_init() {
  HINSTANCE hInstance = GetModuleHandle(NULL);

  // test language
  //SetThreadUILanguage(LANG_ENGLISH);

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

  int screenwidth = GetSystemMetrics(SM_CXSCREEN);
  int screenheight = GetSystemMetrics(SM_CYSCREEN);


#if FULLSCREEN
  RECT rect = {0, 0, screenwidth, screenheight};
  uint style = WS_POPUP;

  AdjustWindowRect(&rect, style, FALSE);

  // create window
  mainhwnd = CreateWindow("FreePianoMainWindow", APP_NAME, style,
                          rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                          NULL, NULL, hInstance, NULL);
#else
  RECT rect;
  rect.left = (screenwidth - display_get_width()) / 2;
  rect.top = (screenheight - display_get_height()) / 2;
  rect.right = rect.left + display_get_width();
  rect.bottom = rect.top + display_get_height();

  uint style = WS_OVERLAPPEDWINDOW;

  AdjustWindowRect(&rect, style, TRUE);

  // create window
  mainhwnd = CreateWindow("FreePianoMainWindow", APP_NAME, style,
                          rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                          NULL, menu_main, hInstance, NULL);
#endif

  if (mainhwnd == NULL) {
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
void gui_shutdown() {
  menu_shutdown();

  if (mainhwnd) {
    DestroyWindow(mainhwnd);
    mainhwnd = NULL;
  }
}

// get main window handle
HWND gui_get_window() {
  return mainhwnd;
}

// show gui
void gui_show() {
  ShowWindow(mainhwnd, SW_SHOW);
  SetActiveWindow(mainhwnd);
  SetForegroundWindow(mainhwnd);
  display_render();
}

// get selected key
int gui_get_selected_key() {
  return preview_key;
}

static HWND export_hwnd = NULL;

static INT_PTR CALLBACK export_progress_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
   case WM_INITDIALOG: {
     // output volume slider
     HWND progress_bar = GetDlgItem(hWnd, IDC_EXPORT_PROGRESS);

     SendMessage(progress_bar, PBM_SETRANGE,
                 (WPARAM) TRUE,                  // redraw flag
                 (LPARAM) MAKELONG(0, 100));     // min. & max. positions

     export_hwnd = hWnd;
   }
   break;

   case WM_COMMAND:
     switch (LOWORD(wParam)) {
      case IDCANCEL:
        EndDialog(hWnd, 0);
        break;
     }
     break;

   case WM_CLOSE:
     EndDialog(hWnd, 0);
     break;

   case WM_DESTROY:
     export_hwnd = NULL;
     break;
  }
  return 0;
}

// show export progress
int gui_show_export_progress() {
  return DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_EXPORTING), gui_get_window(), export_progress_proc);
}

// update progress
void gui_update_export_progress(int progress) {
  // output volume slider
  HWND progress_bar = GetDlgItem(export_hwnd, IDC_EXPORT_PROGRESS);
  PostMessage(progress_bar, PBM_SETPOS, (WPARAM)progress, 0);
}

// close hide progress
void gui_close_export_progress() {
  PostMessage(export_hwnd, WM_CLOSE, 0, 0);
}

// is exporting
bool gui_is_exporting() {
  return export_hwnd != NULL;
}