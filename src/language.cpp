#include "pch.h"
#include "language.h"

static int lang_current;
static const char* string_names[FP_IDS_COUNT];
static const char* string_table[FP_LANG_COUNT][FP_IDS_COUNT];

static WORD system_language(int lang_id) {
  switch (lang_id) {
  case FP_LANG_ENGLISH:  return MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL);
  case FP_LANG_SCHINESE: return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
  }
  return MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
}

void lang_init() {
  // initializie language table.
  for (int lang = 0; lang < FP_LANG_COUNT; lang++) {
    for (int id = 0; id < FP_IDS_COUNT; id++) {
      string_table[lang][id] = "";
      string_names[id] = "";
    }
  }

#define STR_ENGLISH(id, str) string_table[FP_LANG_ENGLISH][id] = str; string_names[id] = #id;
#define STR_SCHINESE(id, str) string_table[FP_LANG_SCHINESE][id] = str;
#include "language_strdef.h"
#undef STR_ENGLISH
#undef STR_SCHINESE

  // auto choose language by default
  lang_set_current(FP_LANG_AUTO);
}

// get default language
int lang_get_default() {
  LANGID id = LANGIDFROMLCID(GetThreadLocale());
  switch (PRIMARYLANGID(id)) {
  case LANG_ENGLISH:
    return FP_LANG_ENGLISH;

  case LANG_CHINESE:
    return FP_LANG_SCHINESE;
  }
  return FP_LANG_ENGLISH;
}

// select current language
void lang_set_current(int languageid) {
  if (languageid > FP_LANG_AUTO && languageid < FP_LANG_COUNT) {
    lang_current = languageid;
  }
  else {
    lang_current = lang_get_default();
  }
}

// get current language
int lang_get_current() {
  return lang_current;
}

// load str
const char* lang_load_string(uint uid) {
  if (uid < FP_IDS_COUNT) {
    return string_table[lang_current][uid];
  }
  return "";
}

// load lang string array
const char* * lang_load_string_array(uint uid) {
  static char temp[1024];
  static char *arr[256];
  const uint temp_size = sizeof(temp) / sizeof(temp[0]);
  uint len = LoadString(GetModuleHandle(NULL), uid, temp,  temp_size - 1);
  temp[len < temp_size ? len : 0] = 0;

  arr[0] = temp;
  uint arrlen = 1;

  for (uint i = 0; i < len; i++) {
    if (temp[i] == ',') {
      temp[i] = '\0';
      if (arrlen < 256) {
        arr[arrlen] = temp + i + 1;
        arrlen++;
      }
    }
  }
  arr[arrlen] = 0;
  return (const char * *)arr;
}


// format lang str
int lang_format_string(char *buff, size_t size, uint strid, ...) {
  const char *format = lang_load_string(strid);
  va_list args;
  va_start(args, strid);
  int result = _vsnprintf(buff, size, format, args);
  va_end(args);
  return result;
}


static struct lang_text_t {
  char *data;
  char *end;
  HGLOBAL hrc;
}
lang_text = { NULL, NULL, NULL };

// open text
int lang_text_open(uint textid) {
  lang_text_close();

  HINSTANCE module = GetModuleHandle(NULL);
  HRSRC hrsrc = FindResourceEx(module, "TEXT", MAKEINTRESOURCE(textid), system_language(lang_current));

  if (hrsrc == NULL)
    hrsrc = FindResourceEx(module, "TEXT", MAKEINTRESOURCE(textid), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));

  if (hrsrc) {
    // load resource
    HGLOBAL hrc = LoadResource(0, hrsrc);

    if (hrc) {
      lang_text.hrc = LoadResource(0, hrsrc);
      lang_text.data = (char *)LockResource(hrc);
      lang_text.end = lang_text.data + SizeofResource(NULL, hrsrc);
      return lang_text.data - lang_text.end;
    }
  }

  return 0;
}

// readline
int lang_text_readline(char *line, size_t size) {
  char *line_end = line;

  while (lang_text.data < lang_text.end) {
    if (*lang_text.data == '\n') {
      if (line_end > line) {
        *line_end = 0;
        lang_text.data++;
        return line_end - line;
      }
    } else   {
      if (line_end < line + size - 1)
        *line_end++ = *lang_text.data;
    }

    lang_text.data++;
  }

  if (line_end > line) {
    *line_end = 0;
    return line_end - line;
  }

  return 0;
}

// close text
void lang_text_close() {
  if (lang_text.hrc)
    FreeResource(lang_text.hrc);

  lang_text.data = NULL;
  lang_text.end = NULL;
  lang_text.hrc = NULL;
}


// error message
static char error_message[1024] = "Unknown error.";

// set last error
void lang_set_last_error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vsprintf_s(error_message, format, args);
  va_end(args);
  fputs(error_message, stderr);
}

// set last error
void lang_set_last_error(uint id, ...) {
  const char *format = lang_load_string(id);
  va_list args;
  va_start(args, id);
  vsprintf_s(error_message, format, args);
  va_end(args);
  fputs(error_message, stderr);
}

// get last error
const char * lang_get_last_error() {
  return error_message;
}


static BOOL CALLBACK localize_hwnd(HWND hwnd, LPARAM lParam) {
  char className[256];
  char text[32];
  GetClassName(hwnd, className, ARRAYSIZE(className));
  GetWindowText(hwnd, text, ARRAYSIZE(text));

  for (int i = 1; i < FP_IDS_COUNT; i++) {
    if (strcmp(text, string_names[i]) == 0) {
      SetWindowText(hwnd, lang_load_string(i));
      return TRUE;
    }
  }

  return TRUE;
}

// localize dialog
void lang_localize_dialog(HWND hwnd) {
  localize_hwnd(hwnd, NULL);
  EnumChildWindows(hwnd, &localize_hwnd, NULL);
}