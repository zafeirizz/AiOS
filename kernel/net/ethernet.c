#include <stdint.h>
#include "../../include/ethernet.h"
#include "../../include/net.h"
#include "../../include/arp.h"
#include "../../include/ip.h"
#include "../../include/rtl8139.h"
#include "../../include/string.h"

#define MAX_FRAME 1518

static uint8_t frame_buf[MAX_FRAME];

static const uint8_t broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void ethernet_init(void) {
    /* Nothing extra needed — rtl8139_init already ran */
}

void ethernet_send(const uint8_t* dst_mac, uint16_t ethertype,
                   const void* payload, uint16_t len) {
    if (len > MAX_FRAME - sizeof(eth_hdr_t)) return;

    eth_hdr_t* hdr = (eth_hdr_t*)frame_buf;
    const uint8_t* src = rtl8139_mac();

    if (dst_mac)
        memcpy(hdr->dst, dst_mac, 6);
    else
        memcpy(hdr->dst, broadcast_mac, 6);

    memcpy(hdr->src, src, 6);
    hdr->type = htons(ethertype);

    memcpy(frame_buf + sizeof(eth_hdr_t), payload, len);
    rtl8139_send(frame_buf, (uint16_t)(sizeof(eth_hdr_t) + len));
}

void ethernet_poll(void) {
    static uint8_t pkt[MAX_FRAME];
    uint16_t len;

    while ((len = rtl8139_recv(pkt, sizeof(pkt))) > 0) {
        if (len < (uint16_t)sizeof(eth_hdr_t)) continue;

        eth_hdr_t* hdr = (eth_hdr_t*)pkt;
        uint16_t type  = ntohs(hdr->type);
        uint8_t* payload = pkt + sizeof(eth_hdr_t);
        uint16_t plen    = len - (uint16_t)sizeof(eth_hdr_t);

        switch (type) {
            case ETH_P_ARP:
                arp_handle(payload, plen);
                break;
            case ETH_P_IP:
                ip_handle(payload, plen);
                break;
            /* drop everything else silently */
        }
    }
}
