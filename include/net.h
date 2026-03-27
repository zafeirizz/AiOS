#ifndef NET_H
#define NET_H

#include <stdint.h>

/* ── Byte-order helpers (we are little-endian x86) ────── */
static inline uint16_t htons(uint16_t v) { return (uint16_t)((v>>8)|(v<<8)); }
static inline uint16_t ntohs(uint16_t v) { return htons(v); }
static inline uint32_t htonl(uint32_t v) {
    return ((v>>24)&0xFF) | ((v>>8)&0xFF00) |
           ((v<<8)&0xFF0000) | ((v<<24)&0xFF000000);
}
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

/* ── MAC / IP helpers ─────────────────────────────────── */
#define IP4(a,b,c,d) ((uint32_t)((a)|(b)<<8|(c)<<16|(d)<<24))
#define MAC_FMT      "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ARG(m)   (m)[0],(m)[1],(m)[2],(m)[3],(m)[4],(m)[5]

/* ── Ethernet ─────────────────────────────────────────── */
#define ETH_ALEN   6
#define ETH_P_IP   0x0800
#define ETH_P_ARP  0x0806

typedef struct {
    uint8_t  dst[ETH_ALEN];
    uint8_t  src[ETH_ALEN];
    uint16_t type;           /* network byte order */
} __attribute__((packed)) eth_hdr_t;

/* ── ARP ──────────────────────────────────────────────── */
#define ARP_REQUEST  1
#define ARP_REPLY    2

typedef struct {
    uint16_t hw_type;        /* 1 = Ethernet          */
    uint16_t proto_type;     /* 0x0800 = IPv4         */
    uint8_t  hw_len;         /* 6                     */
    uint8_t  proto_len;      /* 4                     */
    uint16_t operation;      /* 1=request 2=reply     */
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
} __attribute__((packed)) arp_pkt_t;

/* ── IPv4 ─────────────────────────────────────────────── */
#define IP_PROTO_ICMP  1
#define IP_PROTO_UDP   17
#define IP_PROTO_TCP   6

typedef struct {
    uint8_t  ver_ihl;        /* version=4, IHL=5 (no options) */
    uint8_t  dscp_ecn;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed)) ip_hdr_t;

/* ── ICMP ─────────────────────────────────────────────── */
#define ICMP_ECHO_REQUEST  8
#define ICMP_ECHO_REPLY    0

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_hdr_t;

/* ── UDP ──────────────────────────────────────────────── */
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_hdr_t;

/* ── Checksum ─────────────────────────────────────────── */
uint16_t net_checksum(const void* data, uint32_t len);

#endif
