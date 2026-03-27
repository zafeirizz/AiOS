#include <stdint.h>
#include "../../include/arp.h"
#include "../../include/net.h"
#include "../../include/ethernet.h"
#include "../../include/rtl8139.h"
#include "../../include/ip.h"
#include "../../include/string.h"
#include "../../include/vga.h"

#define ARP_CACHE_SIZE 16

typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    int      valid;
} arp_entry_t;

static arp_entry_t cache[ARP_CACHE_SIZE];

void arp_init(void) {
    memset(cache, 0, sizeof(cache));
}

void arp_insert(uint32_t ip, const uint8_t* mac) {
    /* Update existing entry */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (cache[i].valid && cache[i].ip == ip) {
            memcpy(cache[i].mac, mac, 6);
            return;
        }
    }
    /* Find free slot */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!cache[i].valid) {
            cache[i].ip    = ip;
            cache[i].valid = 1;
            memcpy(cache[i].mac, mac, 6);
            return;
        }
    }
    /* Evict slot 0 (simple FIFO) */
    cache[0].ip    = ip;
    cache[0].valid = 1;
    memcpy(cache[0].mac, mac, 6);
}

const uint8_t* arp_lookup(uint32_t ip) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
        if (cache[i].valid && cache[i].ip == ip)
            return cache[i].mac;

    /* Not found — send ARP request */
    arp_pkt_t req;
    memset(&req, 0, sizeof(req));
    req.hw_type    = htons(1);
    req.proto_type = htons(0x0800);
    req.hw_len     = 6;
    req.proto_len  = 4;
    req.operation  = htons(ARP_REQUEST);
    memcpy(req.sender_mac, rtl8139_mac(), 6);
    req.sender_ip  = ip_our_ip;
    memset(req.target_mac, 0, 6);
    req.target_ip  = ip;

    ethernet_send(NULL, ETH_P_ARP, &req, sizeof(req));
    return NULL;
}

void arp_handle(const void* pkt, uint16_t len) {
    if (len < (uint16_t)sizeof(arp_pkt_t)) return;
    const arp_pkt_t* a = (const arp_pkt_t*)pkt;

    if (ntohs(a->hw_type)    != 1)      return;
    if (ntohs(a->proto_type) != 0x0800) return;

    /* Cache sender regardless of request/reply */
    arp_insert(a->sender_ip, a->sender_mac);

    /* If it's a request for our IP, send a reply */
    if (ntohs(a->operation) == ARP_REQUEST && a->target_ip == ip_our_ip) {
        arp_pkt_t reply;
        reply.hw_type    = htons(1);
        reply.proto_type = htons(0x0800);
        reply.hw_len     = 6;
        reply.proto_len  = 4;
        reply.operation  = htons(ARP_REPLY);
        memcpy(reply.sender_mac, rtl8139_mac(), 6);
        reply.sender_ip  = ip_our_ip;
        memcpy(reply.target_mac, a->sender_mac, 6);
        reply.target_ip  = a->sender_ip;
        ethernet_send(a->sender_mac, ETH_P_ARP, &reply, sizeof(reply));
    }
}

void arp_print_table(void) {
    terminal_writeline("ARP cache:");
    int any = 0;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!cache[i].valid) continue;
        any = 1;
        uint32_t ip = cache[i].ip;
        terminal_write("  ");
        terminal_write_dec((ip)      & 0xFF); terminal_putchar('.');
        terminal_write_dec((ip >> 8) & 0xFF); terminal_putchar('.');
        terminal_write_dec((ip >>16) & 0xFF); terminal_putchar('.');
        terminal_write_dec((ip >>24) & 0xFF);
        terminal_write("  ->  ");
        for (int j = 0; j < 6; j++) {
            terminal_write_hex(cache[i].mac[j]);
            if (j < 5) terminal_putchar(':');
        }
        terminal_putchar('\n');
    }
    if (!any) terminal_writeline("  (empty)");
}
