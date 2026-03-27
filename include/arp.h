#ifndef ARP_H
#define ARP_H

#include <stdint.h>

/* Initialise ARP table */
void arp_init(void);

/* Handle an incoming ARP packet (call from ethernet layer) */
void arp_handle(const void* pkt, uint16_t len);

/*
 * Look up MAC for an IP. Returns pointer to 6-byte MAC if known,
 * NULL if not in cache. Sends an ARP request if not cached.
 */
const uint8_t* arp_lookup(uint32_t ip);

/* Directly insert a known mapping (e.g. gateway) */
void arp_insert(uint32_t ip, const uint8_t* mac);

/* Print the ARP table */
void arp_print_table(void);

#endif
