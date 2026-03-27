#include <stdint.h>
#include "../../include/udp.h"
#include "../../include/net.h"
#include "../../include/ip.h"
#include "../../include/string.h"

#define MAX_BINDINGS 8

typedef struct {
    uint16_t      port;
    udp_handler_t handler;
} udp_binding_t;

static udp_binding_t bindings[MAX_BINDINGS];
static int           nb = 0;

void udp_bind(uint16_t port, udp_handler_t handler) {
    if (nb < MAX_BINDINGS) {
        bindings[nb].port    = port;
        bindings[nb].handler = handler;
        nb++;
    }
}

void udp_handle(uint32_t src_ip, const void* pkt, uint16_t len) {
    if (len < (uint16_t)sizeof(udp_hdr_t)) return;
    const udp_hdr_t* hdr = (const udp_hdr_t*)pkt;
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint16_t src_port = ntohs(hdr->src_port);
    const uint8_t* data = (const uint8_t*)pkt + sizeof(udp_hdr_t);
    uint16_t dlen = (uint16_t)(ntohs(hdr->length) - sizeof(udp_hdr_t));

    for (int i = 0; i < nb; i++) {
        if (bindings[i].port == 0 || bindings[i].port == dst_port) {
            bindings[i].handler(src_ip, src_port, data, dlen);
            return;
        }
    }
}

void udp_send(uint32_t dst_ip, uint16_t dst_port,
              uint16_t src_port, const void* data, uint16_t len) {
    static uint8_t buf[1472];   /* 1500 - 20 IP - 8 UDP */
    udp_hdr_t* hdr = (udp_hdr_t*)buf;

    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->length   = htons((uint16_t)(sizeof(udp_hdr_t) + len));
    hdr->checksum = 0;   /* optional for IPv4 */

    memcpy(buf + sizeof(udp_hdr_t), data, len);
    ip_send(dst_ip, IP_PROTO_UDP, buf,
            (uint16_t)(sizeof(udp_hdr_t) + len));
}
