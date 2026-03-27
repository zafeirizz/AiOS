#ifndef STRING_H
#define STRING_H

/* Use stddef.h only — it gives us size_t without pulling in libc string.h */
#include <stddef.h>

int    strcmp (const char* a, const char* b);
int    strncmp(const char* a, const char* b, size_t n);
size_t strlen (const char* s);

/* Standard signatures — must match what GCC's built-ins expect */
void*  memset (void* dst, int val, size_t n);
void*  memcpy (void* dst, const void* src, size_t n);
int    memcmp (const void* a, const void* b, size_t n);
char*  strcpy (char* dst, const char* src);
char*  strncpy(char* dst, const char* src, size_t n);
char*  strcat (char* dst, const char* src);

/* Additional string functions */
char*  strchr (const char* s, int c);
char*  strstr (const char* haystack, const char* needle);
int    snprintf(char* buf, size_t size, const char* fmt, ...);

#endif
