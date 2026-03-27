#include <stdint.h>
#include "../../include/net.h"
#include "../../include/ip.h"

/* Global network config — defined here, declared extern in ip.h */
uint32_t ip_our_ip  = 0;
uint32_t ip_gateway = 0;
uint32_t ip_netmask = 0;

/* RFC 1071 one's-complement checksum */
uint16_t net_checksum(const void* data, uint32_t len) {
    const uint16_t* p = (const uint16_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len) sum += *(const uint8_t*)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}
