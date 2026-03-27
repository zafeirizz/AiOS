#include <stdint.h>
#include <stddef.h>
#include "../../include/http.h"
#include "../../include/dns.h"
#include "../../include/tcp.h"
#include "../../include/ip.h"
#include "../../include/string.h"
#include "../../include/heap.h"

int http_parse_url(const char* url, http_request_t* req) {
    if (strncmp(url, "http://", 7) != 0) return -1;
    const char* host_start = url + 7;
    const char* colon = strchr(host_start, ':');
    const char* slash = strchr(host_start, '/');
    if (!slash) slash = host_start + strlen(host_start);
    if (colon && colon < slash) {
        int host_len = (int)(colon - host_start);
        if (host_len >= (int)sizeof(req->host)) return -1;
        memcpy(req->host, host_start, host_len);
        req->host[host_len] = '\0';
        req->port = 0;
        const char* pp = colon + 1;
        while (pp < slash && *pp >= '0' && *pp <= '9')
            req->port = req->port * 10 + (*pp++ - '0');
        if (req->port == 0) req->port = 80;
    } else {
        int host_len = (int)(slash - host_start);
        if (host_len >= (int)sizeof(req->host)) return -1;
        memcpy(req->host, host_start, host_len);
        req->host[host_len] = '\0';
        req->port = 80;
    }
    if (*slash == '\0') { req->path[0] = '/'; req->path[1] = '\0'; }
    else {
        int plen = (int)strlen(slash);
        if (plen >= (int)sizeof(req->path)) return -1;
        memcpy(req->path, slash, plen + 1);
    }
    int ulen = (int)strlen(url);
    if (ulen >= (int)sizeof(req->url)) return -1;
    memcpy(req->url, url, ulen + 1);
    memcpy(req->method, "GET", 4);
    return 0;
}

int http_get(const char* url, http_response_t* response) {
    memset(response, 0, sizeof(*response));
    http_request_t req;
    if (http_parse_url(url, &req) != 0) return -1;
    uint32_t server_ip = 0;
    if (dns_resolve(req.host, &server_ip) != 0) return -2;
    tcp_socket_t* sock = tcp_socket_create();
    if (!sock) return -3;
    if (tcp_connect(sock, server_ip, req.port) != 0) { tcp_close(sock); return -4; }
    char request[512];
    int len = snprintf(request, sizeof(request),
        "%s %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: QifshaOS/1.0\r\nConnection: close\r\n\r\n",
        req.method, req.path, req.host);
    if (len >= (int)sizeof(request)) { tcp_close(sock); return -5; }
    if (tcp_send(sock, request, len) != len) { tcp_close(sock); return -6; }
    static char buffer[8192];
    int total = 0, header_end = -1, content_length = -1;
    char* body_start = (void*)0;
    while (total < (int)sizeof(buffer) - 1) {
        int n = tcp_recv(sock, buffer + total, (int)sizeof(buffer) - total - 1);
        if (n <= 0) break;
        total += n;
        buffer[total] = '\0';
        if (header_end == -1) {
            char* he = strstr(buffer, "\r\n\r\n");
            if (he) {
                header_end = (int)(he - buffer) + 4;
                body_start = he + 4;
                char* sp1 = strchr(buffer, ' ');
                if (sp1) {
                    response->status_code = 0;
                    char* sp2 = strchr(sp1+1, ' ');
                    if (sp2) {
                        for (char* p = sp1+1; p < sp2; p++)
                            if (*p>='0'&&*p<='9') response->status_code=response->status_code*10+(*p-'0');
                        int slen=(int)(sp2-(sp1+1));
                        if(slen<(int)sizeof(response->status_text)){memcpy(response->status_text,sp1+1,slen);response->status_text[slen]='\0';}
                    }
                }
                char* cl = strstr(buffer, "Content-Length:");
                if (cl && cl < he) {
                    cl += 15; while(*cl==' ')cl++;
                    content_length = 0;
                    while(*cl>='0'&&*cl<='9') content_length=content_length*10+(*cl++- '0');
                }
                char* ct = strstr(buffer, "Content-Type:");
                if (ct && ct < he) {
                    ct += 13; while(*ct==' ')ct++;
                    char* cte = strchr(ct,'\r');
                    if(cte){int ctlen=(int)(cte-ct);if(ctlen<(int)sizeof(response->content_type)){memcpy(response->content_type,ct,ctlen);response->content_type[ctlen]='\0';}}
                }
            }
        }
        if (header_end != -1 && content_length != -1 && total - header_end >= content_length) break;
    }
    if (body_start) {
        response->body_len = total - (int)(body_start - buffer);
        response->body = (char*)kmalloc(response->body_len + 1);
        if (response->body) { memcpy(response->body, body_start, response->body_len); response->body[response->body_len] = '\0'; }
        response->content_length = content_length;
    }
    tcp_close(sock);
    return 0;
}

void http_free_response(http_response_t* response) {
    if (response->body) { kfree(response->body); response->body = (void*)0; }
    response->body_len = 0;
}
