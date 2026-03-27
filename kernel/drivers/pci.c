#include <stdint.h>
#include "../../include/pci.h"

#define PCI_ADDR  0xCF8
#define PCI_DATA  0xCFC

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0,%1"::"a"(val),"Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(port)); return v;
}

static uint32_t pci_addr(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg) {
    return (1u<<31) | ((uint32_t)bus<<16) | ((uint32_t)(dev&0x1F)<<11) |
           ((uint32_t)(fn&0x7)<<8) | (reg & 0xFC);
}

uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg) {
    outl(PCI_ADDR, pci_addr(bus, dev, fn, reg));
    return inl(PCI_DATA);
}

void pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg, uint32_t val) {
    outl(PCI_ADDR, pci_addr(bus, dev, fn, reg));
    outl(PCI_DATA, val);
}

uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg) {
    uint32_t v = pci_read32(bus, dev, fn, reg & 0xFC);
    return (uint16_t)(v >> ((reg & 2) * 8));
}

uint8_t pci_read8(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg) {
    uint32_t v = pci_read32(bus, dev, fn, reg & 0xFC);
    return (uint8_t)(v >> ((reg & 3) * 8));
}

int pci_find_device(uint16_t vendor, uint16_t device,
                    uint8_t* bus_out, uint8_t* dev_out) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read32((uint8_t)bus, dev, 0, 0x00);
            if ((id & 0xFFFF) == vendor && ((id >> 16) & 0xFFFF) == device) {
                *bus_out = (uint8_t)bus;
                *dev_out = dev;
                return 1;
            }
        }
    }
    return 0;
}

void pci_enable_busmaster(uint8_t bus, uint8_t dev, uint8_t fn) {
    uint32_t cmd = pci_read32(bus, dev, fn, 0x04);
    cmd |= (1<<2);   /* Bus Master Enable */
    cmd |= (1<<0);   /* I/O Space Enable  */
    pci_write32(bus, dev, fn, 0x04, cmd);
}

uint32_t pci_bar0_io(uint8_t bus, uint8_t dev, uint8_t fn) {
    uint32_t bar = pci_read32(bus, dev, fn, 0x10);
    return bar & ~0x3u;   /* mask type bits */
}
