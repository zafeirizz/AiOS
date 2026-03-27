#include <stdint.h>
#include "../../include/icmp.h"
#include "../../include/net.h"
#include "../../include/ip.h"
#include "../../include/string.h"
#include "../../include/vga.h"

static volatile int      got_reply  = 0;
static volatile uint32_t reply_ip   = 0;
static volatile uint16_t reply_seq  = 0;

void icmp_ping(uint32_t dst_ip, uint16_t seq) {
    static uint8_t buf[sizeof(icmp_hdr_t) + 32];
    icmp_hdr_t* hdr = (icmp_hdr_t*)buf;

    hdr->type     = ICMP_ECHO_REQUEST;
    hdr->code     = 0;
    hdr->checksum = 0;
    hdr->id       = htons(0x1337);
    hdr->seq      = htons(seq);

    /* Fill payload with 'A' */
    memset(buf + sizeof(icmp_hdr_t), 'A', 32);

    hdr->checksum = net_checksum(buf, sizeof(buf));
    ip_send(dst_ip, IP_PROTO_ICMP, buf, (uint16_t)sizeof(buf));
}

void icmp_handle(uint32_t src_ip, const void* pkt, uint16_t len) {
    if (len < (uint16_t)sizeof(icmp_hdr_t)) return;
    const icmp_hdr_t* hdr = (const icmp_hdr_t*)pkt;

    if (hdr->type == ICMP_ECHO_REPLY) {
        got_reply = 1;
        reply_ip  = src_ip;
        reply_seq = ntohs(hdr->seq);
        return;
    }

    /* Respond to echo requests directed at us */
    if (hdr->type == ICMP_ECHO_REQUEST) {
        static uint8_t reply[1480];
        icmp_hdr_t* r = (icmp_hdr_t*)reply;
        memcpy(reply, pkt, len);
        r->type     = ICMP_ECHO_REPLY;
        r->checksum = 0;
        r->checksum = net_checksum(reply, len);
        ip_send(src_ip, IP_PROTO_ICMP, reply, len);
    }
}

int icmp_got_reply(uint32_t* from_ip, uint16_t* seq_out) {
    if (!got_reply) return 0;
    if (from_ip)  *from_ip  = reply_ip;
    if (seq_out)  *seq_out  = reply_seq;
    got_reply = 0;
    return 1;
}
