#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>
#include "tcp.h"

/* HTTP Request/Response structures */
typedef struct {
    char method[8];      /* GET, POST, etc. */
    char url[256];       /* Request URL */
    char host[64];       /* Host header */
    char path[192];      /* Path part of URL */
    uint16_t port;       /* Port (usually 80) */
} http_request_t;

typedef struct {
    int status_code;     /* HTTP status code */
    char status_text[64]; /* Status text */
    char content_type[64]; /* Content-Type header */
    int content_length;  /* Content-Length header */
    char* body;          /* Response body */
    int body_len;        /* Body length */
} http_response_t;

/* HTTP Functions */
int http_get(const char* url, http_response_t* response);
int http_parse_url(const char* url, http_request_t* req);
void http_free_response(http_response_t* response);

#endif