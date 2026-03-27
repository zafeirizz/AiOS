#ifndef TCP_H
#define TCP_H

#include <stdint.h>
#include "ip.h"

/* TCP Header */
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;  /* 4 bits */
    uint8_t  flags;        /* 6 bits */
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

/* TCP Flags */
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20

/* TCP States */
typedef enum {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT
} tcp_state_t;

/* TCP Socket */
typedef struct tcp_socket {
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    tcp_state_t state;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t window;
    struct tcp_socket* next;
} tcp_socket_t;

/* TCP Functions */
void tcp_init(void);
tcp_socket_t* tcp_socket_create(void);
void tcp_socket_bind(tcp_socket_t* sock, uint16_t port);
int tcp_connect(tcp_socket_t* sock, uint32_t ip, uint16_t port);
int tcp_send(tcp_socket_t* sock, const void* data, uint16_t len);
int tcp_recv(tcp_socket_t* sock, void* buf, uint16_t buf_len);
void tcp_close(tcp_socket_t* sock);
void tcp_handle(uint32_t src_ip, const void* payload, uint16_t len);

#endif