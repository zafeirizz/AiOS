/* No system headers — freestanding environment only */
#include "../../include/string.h"

int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && *b && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

size_t strlen(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

void* memset(void* dst, int val, size_t n) {
    unsigned char* p = (unsigned char*)dst;
    while (n--) *p++ = (unsigned char)val;
    return dst;
}

void* memcpy(void* dst, const void* src, size_t n) {
    unsigned char*       d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    while (n--) {
        if (*pa != *pb) return (int)*pa - (int)*pb;
        pa++; pb++;
    }
    return 0;
}

char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++));
    return dst;
}

char* strncpy(char* dst, const char* src, size_t n) {
    char* d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dst;
}

char* strcat(char* dst, const char* src) {
    char* d = dst;
    while (*d) d++;  /* Find end of dst */
    while ((*d++ = *src++));  /* Copy src to end of dst */
    return dst;
}

/* Additional string functions for HTTP/HTML support */
char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == '\0') ? (char*)s : NULL;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (char*)haystack;
    }
    return NULL;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    /* Simple implementation that only handles %s, %d, and %% */
    if (size == 0) return 0;
    
    char* p = buf;
    char* end = buf + size - 1;
    const char* f = fmt;
    
    /* Simple va_args simulation - this is not standards compliant but works for our use */
    void* args = (void*)&fmt + sizeof(fmt);
    
    while (*f && p < end) {
        if (*f == '%') {
            f++;
            if (*f == 's') {
                const char* s = *(const char**)args;
                args += sizeof(const char*);
                while (*s && p < end) {
                    *p++ = *s++;
                }
            } else if (*f == 'd') {
                int d = *(int*)args;
                args += sizeof(int);
                if (d < 0) {
                    *p++ = '-';
                    d = -d;
                }
                char tmp[16];
                char* t = tmp + sizeof(tmp) - 1;
                *t = '\0';
                do {
                    *--t = '0' + (d % 10);
                    d /= 10;
                } while (d && t > tmp);
                while (*t && p < end) {
                    *p++ = *t++;
                }
            } else if (*f == '%') {
                *p++ = '%';
            }
            f++;
        } else {
            *p++ = *f++;
        }
    }
    *p = '\0';
    return p - buf;
}
