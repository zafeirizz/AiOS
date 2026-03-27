#ifndef UDP_H
#define UDP_H

#include <stdint.h>

/*
 * Callback type for received UDP datagrams.
 * src_ip, src_port: sender info
 * data, len: payload
 */
typedef void (*udp_handler_t)(uint32_t src_ip, uint16_t src_port,
                               const void* data, uint16_t len);

/* Register a handler for a local UDP port (0 = wildcard/all) */
void udp_bind(uint16_t port, udp_handler_t handler);

/* Handle an incoming UDP packet (called by IP layer) */
void udp_handle(uint32_t src_ip, const void* pkt, uint16_t len);

/* Send a UDP datagram */
void udp_send(uint32_t dst_ip, uint16_t dst_port,
              uint16_t src_port, const void* data, uint16_t len);

#endif
