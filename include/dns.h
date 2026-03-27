#ifndef DNS_H
#define DNS_H

#include <stdint.h>

/* DNS resolver over UDP port 53.
 * Sends a real DNS query and waits (spin-polls) for a reply.
 * Returns 0 on success, -1 on failure.
 * resolved_ip is written in host byte order (same as ip_our_ip). */
int  dns_resolve(const char* hostname, uint32_t* resolved_ip);

/* Configure the DNS server IP (host byte order, default 8.8.8.8) */
void dns_set_server(uint32_t server_ip);

/* Called by UDP layer when a packet arrives on port 53 */
void dns_handle_reply(uint32_t src_ip, uint16_t src_port,
                      const void* data, uint16_t len);

/* Simple string -> IP parser ("1.2.3.4") returns 0 on bad format */
int  dns_parse_ip(const char* s, uint32_t* out);

#endif
