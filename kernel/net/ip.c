#include <stdint.h>
#include "../../include/ip.h"
#include "../../include/net.h"
#include "../../include/arp.h"
#include "../../include/icmp.h"
#include "../../include/udp.h"
#include "../../include/tcp.h"
#include "../../include/ethernet.h"
#include "../../include/string.h"

static uint16_t ip_id = 1;

void ip_init(uint32_t our_ip, uint32_t gateway, uint32_t netmask) {
    ip_our_ip  = our_ip;
    ip_gateway = gateway;
    ip_netmask = netmask;
}

void ip_send(uint32_t dst_ip, uint8_t protocol,
             const void* payload, uint16_t payload_len) {
    static uint8_t pkt[1500];
    ip_hdr_t* hdr = (ip_hdr_t*)pkt;

    hdr->ver_ihl    = 0x45;   /* IPv4, IHL=5 (20 bytes, no options) */
    hdr->dscp_ecn   = 0;
    hdr->total_len  = htons((uint16_t)(sizeof(ip_hdr_t) + payload_len));
    hdr->id         = htons(ip_id++);
    hdr->flags_frag = 0;
    hdr->ttl        = 64;
    hdr->protocol   = protocol;
    hdr->checksum   = 0;
    hdr->src_ip     = ip_our_ip;
    hdr->dst_ip     = dst_ip;
    hdr->checksum   = net_checksum(hdr, sizeof(ip_hdr_t));

    memcpy(pkt + sizeof(ip_hdr_t), payload, payload_len);

    /* Route: same subnet → direct, else → gateway */
    uint32_t next_hop = ((dst_ip & ip_netmask) == (ip_our_ip & ip_netmask))
                        ? dst_ip : ip_gateway;

    const uint8_t* dst_mac = arp_lookup(next_hop);
    if (!dst_mac) {
        /* ARP request sent; packet dropped this time.
           Upper layer (e.g. ping) retries on next call. */
        return;
    }

    ethernet_send(dst_mac, ETH_P_IP, pkt,
                  (uint16_t)(sizeof(ip_hdr_t) + payload_len));
}

void ip_handle(const void* pkt, uint16_t len) {
    if (len < (uint16_t)sizeof(ip_hdr_t)) return;
    const ip_hdr_t* hdr = (const ip_hdr_t*)pkt;

    /* Accept only IPv4, IHL=5, destined for us or broadcast */
    if ((hdr->ver_ihl >> 4) != 4) return;
    if ((hdr->ver_ihl & 0xF) != 5) return;
    if (hdr->dst_ip != ip_our_ip && hdr->dst_ip != 0xFFFFFFFF) return;

    const uint8_t* payload = (const uint8_t*)pkt + sizeof(ip_hdr_t);
    uint16_t plen = (uint16_t)(ntohs(hdr->total_len) - sizeof(ip_hdr_t));

    switch (hdr->protocol) {
        case IP_PROTO_ICMP:
            icmp_handle(hdr->src_ip, payload, plen);
            break;
        case IP_PROTO_UDP:
            udp_handle(hdr->src_ip, payload, plen);
            break;
        case IP_PROTO_TCP:
            tcp_handle(hdr->src_ip, payload, plen);
            break;
    }
}
