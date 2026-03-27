#include <stdint.h>
#include <stddef.h>
#include "../../include/tcp.h"
#include "../../include/ip.h"
#include "../../include/heap.h"
#include "../../include/ethernet.h"
#include "../../include/string.h"

/* TCP Socket List */
static tcp_socket_t* tcp_sockets = NULL;
static uint16_t next_ephemeral_port = 49152;

/* Static helper functions */
static uint16_t tcp_checksum(tcp_header_t* tcp, uint32_t src_ip, uint32_t dst_ip, uint16_t len) {
    /* Simplified checksum calculation */
    uint32_t sum = 0;
    uint16_t* data = (uint16_t*)(void*)tcp;
    
    /* Pseudo header */
    sum += (src_ip >> 16) + (src_ip & 0xFFFF);
    sum += (dst_ip >> 16) + (dst_ip & 0xFFFF);
    sum += 6;  /* TCP protocol */
    sum += len;
    
    /* TCP header and data */
    for (int i = 0; i < len / 2; i++) {
        sum += data[i];
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

static void tcp_send_packet(tcp_socket_t* sock, uint8_t flags, const void* data, uint16_t data_len) {
    uint16_t total_len = sizeof(tcp_header_t) + data_len;
    uint8_t* packet = (uint8_t*)kmalloc(total_len);
    if (!packet) return;
    
    tcp_header_t* tcp = (tcp_header_t*)packet;
    
    /* Fill TCP header */
    tcp->src_port = sock->local_port;
    tcp->dst_port = sock->remote_port;
    tcp->seq_num = sock->seq_num;
    tcp->ack_num = sock->ack_num;
    tcp->data_offset = 5 << 4;  /* 5 * 4 = 20 bytes */
    tcp->flags = flags;
    tcp->window = sock->window;
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;
    
    /* Copy data */
    if (data && data_len) {
        memcpy(packet + sizeof(tcp_header_t), data, data_len);
    }
    
    /* Calculate checksum */
    tcp->checksum = tcp_checksum(tcp, sock->local_ip, sock->remote_ip, total_len);
    
    /* Send via IP */
    ip_send(sock->remote_ip, IP_PROTO_TCP, packet, total_len);
    
    /* Update sequence number */
    if (data_len > 0 || (flags & TCP_SYN) || (flags & TCP_FIN)) {
        sock->seq_num += data_len + ((flags & TCP_SYN) ? 1 : 0) + ((flags & TCP_FIN) ? 1 : 0);
    }
    
    kfree(packet);
}

static void tcp_send_syn(tcp_socket_t* sock) {
    tcp_send_packet(sock, TCP_SYN, NULL, 0);
}

/* TCP Functions */
void tcp_init(void) {
    /* Initialize TCP socket list */
    tcp_sockets = NULL;
}

tcp_socket_t* tcp_socket_create(void) {
    tcp_socket_t* sock = (tcp_socket_t*)kmalloc(sizeof(tcp_socket_t));
    if (!sock) return NULL;
    
    memset(sock, 0, sizeof(tcp_socket_t));
    sock->state = TCP_CLOSED;
    sock->seq_num = 0x12345678;  /* Random starting sequence */
    sock->ack_num = 0;
    sock->window = 65535;
    
    /* Add to socket list */
    sock->next = tcp_sockets;
    tcp_sockets = sock;
    
    return sock;
}

void tcp_socket_bind(tcp_socket_t* sock, uint16_t port) {
    sock->local_port = port;
}

int tcp_connect(tcp_socket_t* sock, uint32_t ip, uint16_t port) {
    if (sock->state != TCP_CLOSED) return -1;
    
    sock->remote_ip = ip;
    sock->remote_port = port;
    sock->local_port = next_ephemeral_port++;
    sock->state = TCP_SYN_SENT;
    
    /* Send SYN packet */
    tcp_send_syn(sock);
    
    /* Wait for SYN+ACK (simplified - in real implementation we'd wait with timeout) */
    return 0;
}

int tcp_send(tcp_socket_t* sock, const void* data, uint16_t len) {
    if (sock->state != TCP_ESTABLISHED) return -1;
    
    /* Send data packet */
    tcp_send_packet(sock, TCP_ACK | TCP_PSH, data, len);
    return len;
}

int tcp_recv(tcp_socket_t* sock, void* buf, uint16_t buf_len) {
    (void)sock; (void)buf; (void)buf_len;
    /* Simplified - in real implementation we'd have receive buffers */
    return 0;
}

void tcp_close(tcp_socket_t* sock) {
    if (sock->state == TCP_ESTABLISHED) {
        tcp_send_packet(sock, TCP_FIN | TCP_ACK, NULL, 0);
        sock->state = TCP_FIN_WAIT_1;
    }
}

void tcp_handle(uint32_t src_ip, const void* payload, uint16_t len) {
    if (len < sizeof(tcp_header_t)) return;
    
    tcp_header_t* tcp = (tcp_header_t*)payload;
    
    /* Find matching socket */
    tcp_socket_t* sock = tcp_sockets;
    while (sock) {
        if (sock->local_port == tcp->dst_port && 
            sock->remote_port == tcp->src_port &&
            sock->remote_ip == src_ip) {
            break;
        }
        sock = sock->next;
    }
    
    if (!sock) return;
    
    /* Handle packet based on state */
    switch (sock->state) {
        case TCP_SYN_SENT:
            if (tcp->flags & TCP_SYN && tcp->flags & TCP_ACK) {
                /* SYN+ACK received */
                sock->ack_num = tcp->seq_num + 1;
                sock->state = TCP_ESTABLISHED;
                tcp_send_packet(sock, TCP_ACK, NULL, 0);
            }
            break;
            
        case TCP_ESTABLISHED:
            if (tcp->flags & TCP_FIN) {
                /* Connection closing */
                sock->ack_num = tcp->seq_num + 1;
                tcp_send_packet(sock, TCP_ACK, NULL, 0);
                sock->state = TCP_CLOSE_WAIT;
            }
            break;
            
        default:
            /* Handle other states as needed */
            break;
    }
}