#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>

/* Initialise Ethernet layer (call after rtl8139_init) */
void ethernet_init(void);

/* Called from the idle loop / IRQ to process any waiting packets */
void ethernet_poll(void);

/* Send a frame. dst_mac=NULL sends a broadcast. */
void ethernet_send(const uint8_t* dst_mac, uint16_t ethertype,
                   const void* payload, uint16_t len);

#endif
