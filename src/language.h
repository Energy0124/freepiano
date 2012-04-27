#pragma once

// load str
const char * lang_load_string(uint uid);

// load lang filter string
const char * lang_load_filter_string(uint uid);

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