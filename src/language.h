#pragma once

#define FP_LANG_AUTO       -1
#define FP_LANG_ENGLISH     0
#define FP_LANG_SCHINESE    1
#define FP_LANG_COUNT       2

// language string def
#define STR_ENGLISH(id, str)  id,
#define STR_SCHINESE(id, str) 
enum StringIDs {
  FP_IDS_EMPTY,
#include "language_strdef.h"
  FP_IDS_COUNT,
};
#undef STR_ENGLISH
#undef STR_SCHINESE

// initialize languages
void lang_init();

// select current language
void lang_set_current(int languageid);

// get current language
int lang_get_current();

// load str
const char* lang_load_string(uint uid);

// load lang string array
const char** lang_load_string_array(uint uid);

// format lang str
int lang_format_string(char *buff, size_t size, uint strid, ...);

// open text
int lang_text_open(uint textid);

// readline
int lang_text_readline(char *buff, size_t size);

// close text
void lang_text_close();

// set last error
void lang_set_last_error(const char *format, ...);

// set last error
void lang_set_last_error(uint id, ...);

// get last error
const char * lang_get_last_error();

// localize dialog
void lang_localize_dialog(HWND hwnd);