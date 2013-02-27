#include "pch.h"
#include "language.h"

// load str
const char* lang_load_string(uint uid) {
  static char temp[1024];
  const uint temp_size = sizeof(temp) / sizeof(temp[0]);
  uint len = LoadString(GetModuleHandle(NULL), uid, temp,  temp_size - 1);
  temp[len < temp_size ? len : 0] = 0;
  return temp;
}

// load lang filter string
const char* lang_load_filter_string(uint uid) {
  static char temp[1024];
  const uint temp_size = sizeof(temp) / sizeof(temp[0]);
  uint len = LoadString(GetModuleHandle(NULL), uid, temp,  temp_size - 1);
  temp[len < temp_size ? len : 0] = 0;
  for (uint i = 0; i < len; i++)
    if (temp[i] == '|')
      temp[i] = '\0';
  return temp;
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

  HRSRC hrsrc = FindResource(0, MAKEINTRESOURCE(textid), "TEXT");
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

// get last error
const char * lang_get_last_error() {
  return error_message;
}