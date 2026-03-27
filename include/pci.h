#ifndef PCI_H
#define PCI_H

#include <stdint.h>

/* Read/write 32-bit PCI config space */
uint32_t pci_read32 (uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg);
void     pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg, uint32_t val);
uint16_t pci_read16 (uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg);
uint8_t  pci_read8  (uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg);

/* Scan all buses looking for vendor:device. Returns 1 and fills
 * *bus_out, *dev_out if found, else returns 0. */
int pci_find_device(uint16_t vendor, uint16_t device,
                    uint8_t* bus_out, uint8_t* dev_out);

/* Enable bus-mastering DMA for a device */
void pci_enable_busmaster(uint8_t bus, uint8_t dev, uint8_t fn);

/* Return the I/O base address from BAR0 (type 0x01 = I/O space) */
uint32_t pci_bar0_io(uint8_t bus, uint8_t dev, uint8_t fn);

#endif
