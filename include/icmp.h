#ifndef ICMP_H
#define ICMP_H

#include <stdint.h>

/* Handle an incoming ICMP packet (called by IP layer) */
void icmp_handle(uint32_t src_ip, const void* pkt, uint16_t len);

/*
 * Send an ICMP echo request (ping) to dst_ip.
 * seq: sequence number (increment each call)
 * Returns 0 immediately — response comes asynchronously via icmp_handle.
 */
void icmp_ping(uint32_t dst_ip, uint16_t seq);

/* Returns 1 if a ping reply has been received since last check, 0 otherwise.
 * Also fills *from_ip and *seq_out if non-NULL. Clears the flag on read. */
int icmp_got_reply(uint32_t* from_ip, uint16_t* seq_out);

#endif
