#ifndef HTML_H
#define HTML_H

#include <stdint.h>

/* HTML parsing functions */
char* html_extract_text(const char* html, int html_len);
void html_free_text(char* text);

#endif