#ifndef IP_H
#define IP_H

#include <stdint.h>
#include "net.h"

/* Network configuration — set before using IP layer */
extern uint32_t ip_our_ip;
extern uint32_t ip_gateway;
extern uint32_t ip_netmask;

/* Initialise IP layer with static config */
void ip_init(uint32_t our_ip, uint32_t gateway, uint32_t netmask);

/* Handle incoming IP packet from Ethernet layer */
void ip_handle(const void* pkt, uint16_t len);

/*
 * Send an IP packet.
 * payload: data after the IP header
 * payload_len: bytes of payload
 * dst_ip: destination in host byte order
 * protocol: IP_PROTO_ICMP / IP_PROTO_UDP / etc.
 */
void ip_send(uint32_t dst_ip, uint8_t protocol,
             const void* payload, uint16_t payload_len);

#endif
