#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

#define RTL_VENDOR  0x10EC
#define RTL_DEVICE  0x8139

/* Initialise the RTL8139. Returns 0 on success, -1 if not found. */
int  rtl8139_init(void);

/* Send a raw Ethernet frame. len must be <= 1792 bytes. */
void rtl8139_send(const void* data, uint16_t len);

/*
 * Poll for a received packet. Copies data into buf (max buf_len bytes).
 * Returns number of bytes received, or 0 if nothing waiting.
 * Call repeatedly in the idle loop or from the IRQ handler.
 */
uint16_t rtl8139_recv(void* buf, uint16_t buf_len);

/* Returns our MAC address (6 bytes) */
const uint8_t* rtl8139_mac(void);

/* IRQ handler — called by the IRQ dispatch system */
void rtl8139_irq_handler(void);

#endif
