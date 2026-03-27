#include <stdint.h>
#include "../../include/dns.h"
#include "../../include/udp.h"
#include "../../include/ip.h"
#include "../../include/net.h"
#include "../../include/string.h"
#include "../../include/timer.h"
#include "../../include/vga.h"

/* ── DNS wire format helpers ────────────────────────── */

#define DNS_PORT        53
#define DNS_TIMEOUT_MS  3000   /* 3 second timeout */

typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_hdr_t;

/* Default DNS server: 8.8.8.8 */
static uint32_t dns_server_ip = IP4(8, 8, 8, 8);

/* Reply state — set by dns_handle_reply() */
static volatile uint16_t dns_pending_id    = 0;
static volatile uint32_t dns_resolved_ip   = 0;
static volatile int      dns_reply_arrived = 0;

/* ── Public API ─────────────────────────────────────── */

void dns_set_server(uint32_t server_ip) {
    dns_server_ip = server_ip;
}

int dns_parse_ip(const char* s, uint32_t* out) {
    uint32_t ip = 0;
    int parts[4] = {0};
    int part = 0;
    const char* p = s;
    while (*p && part < 4) {
        if (*p >= '0' && *p <= '9') {
            parts[part] = parts[part] * 10 + (*p - '0');
        } else if (*p == '.') {
            part++;
        } else {
            return -1;
        }
        p++;
    }
    if (part != 3) return -1;
    ip = (uint32_t)parts[0]
       | ((uint32_t)parts[1] << 8)
       | ((uint32_t)parts[2] << 16)
       | ((uint32_t)parts[3] << 24);
    *out = ip;
    return 0;
}

/* Encode a hostname into DNS label format in buf, return bytes written */
static int dns_encode_name(const char* hostname, uint8_t* buf) {
    int pos = 0;
    const char* start = hostname;
    while (*start) {
        const char* end = start;
        while (*end && *end != '.') end++;
        int len = (int)(end - start);
        buf[pos++] = (uint8_t)len;
        for (int i = 0; i < len; i++)
            buf[pos++] = (uint8_t)start[i];
        start = (*end == '.') ? end + 1 : end;
    }
    buf[pos++] = 0;   /* root label */
    return pos;
}

/* ── Reply handler (called by UDP layer) ────────────── */

void dns_handle_reply(uint32_t src_ip, uint16_t src_port,
                      const void* data, uint16_t len) {
    (void)src_ip; (void)src_port;

    if (len < sizeof(dns_hdr_t)) return;
    const dns_hdr_t* hdr = (const dns_hdr_t*)data;

    /* Only accept replies matching our pending query */
    uint16_t id = ntohs(hdr->id);
    if (id != dns_pending_id) return;

    /* Check QR bit = 1 (reply) and no error (RCODE == 0) */
    uint16_t flags = ntohs(hdr->flags);
    if (!(flags & 0x8000)) return;           /* Not a reply */
    if ((flags & 0x000F) != 0) return;       /* Error RCODE */
    if (ntohs(hdr->ancount) == 0) return;    /* No answers */

    /* Skip header + question section */
    const uint8_t* p   = (const uint8_t*)data + sizeof(dns_hdr_t);
    const uint8_t* end = (const uint8_t*)data + len;

    /* Skip question: name labels + 4 bytes (type + class) */
    while (p < end && *p != 0) {
        if ((*p & 0xC0) == 0xC0) { p += 2; goto skip_qtype; }
        p += 1 + *p;
    }
    p++;  /* null label */
skip_qtype:
    p += 4;  /* QTYPE + QCLASS */

    /* Parse answer records */
    int ancount = ntohs(hdr->ancount);
    for (int i = 0; i < ancount && p < end; i++) {
        /* Skip name (may be pointer or labels) */
        if (p >= end) break;
        if ((*p & 0xC0) == 0xC0) {
            p += 2;
        } else {
            while (p < end && *p != 0) {
                if ((*p & 0xC0) == 0xC0) { p += 2; goto after_name; }
                p += 1 + *p;
            }
            p++;
        }
after_name:
        if (p + 10 > end) break;
        uint16_t rtype  = (uint16_t)(p[0] << 8 | p[1]);
        /* uint16_t rclass = p[2] << 8 | p[3]; */
        /* uint32_t ttl = ... */
        uint16_t rdlen  = (uint16_t)(p[8] << 8 | p[9]);
        p += 10;

        if (rtype == 1 && rdlen == 4 && p + 4 <= end) {
            /* A record — IPv4 address in network byte order */
            uint32_t ip = (uint32_t)p[0]
                        | ((uint32_t)p[1] << 8)
                        | ((uint32_t)p[2] << 16)
                        | ((uint32_t)p[3] << 24);
            dns_resolved_ip   = ip;
            dns_reply_arrived = 1;
            return;
        }
        p += rdlen;
    }
}

/* ── Main resolver ──────────────────────────────────── */

int dns_resolve(const char* hostname, uint32_t* resolved_ip) {
    /* Try parsing as raw IP first */
    if (dns_parse_ip(hostname, resolved_ip) == 0) return 0;

    /* Build DNS query packet */
    static uint8_t pkt[512];
    static uint16_t txid_counter = 0x1234;
    txid_counter++;
    uint16_t txid = txid_counter;

    dns_hdr_t* hdr  = (dns_hdr_t*)pkt;
    hdr->id         = htons(txid);
    hdr->flags      = htons(0x0100);  /* RD = 1 (recursion desired) */
    hdr->qdcount    = htons(1);
    hdr->ancount    = 0;
    hdr->nscount    = 0;
    hdr->arcount    = 0;

    uint8_t* qp = pkt + sizeof(dns_hdr_t);
    int namelen = dns_encode_name(hostname, qp);
    qp += namelen;
    qp[0] = 0; qp[1] = 1;  /* QTYPE  A */
    qp[2] = 0; qp[3] = 1;  /* QCLASS IN */
    qp += 4;

    uint16_t pkt_len = (uint16_t)(qp - pkt);

    /* Register UDP handler on port 53 (bind wildcard so we also get reply) */
    udp_bind(53, dns_handle_reply);

    /* Send query */
    dns_pending_id    = txid;
    dns_reply_arrived = 0;
    dns_resolved_ip   = 0;

    udp_send(dns_server_ip, DNS_PORT, DNS_PORT, pkt, pkt_len);

    /* Spin-poll for reply (timer_get_ticks() gives ms since boot) */
    uint32_t deadline = timer_get_ticks() + DNS_TIMEOUT_MS;
    while (!dns_reply_arrived && timer_get_ticks() < deadline) {
        /* network polling happens in kernel main loop via rtl8139/e1000 interrupts */
        __asm__ volatile("pause");
    }

    if (!dns_reply_arrived) return -1;

    *resolved_ip = dns_resolved_ip;
    return 0;
}
