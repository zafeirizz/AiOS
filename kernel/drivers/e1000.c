#include <stdint.h>
#include "../../include/e1000.h"
#include "../../include/pci.h"
#include "../../include/heap.h"
#include "../../include/string.h"
#include "../../include/ethernet.h"

/* e1000 device instance */
static e1000_device_t e1000_dev;

/* Read from e1000 register */
static uint32_t e1000_read_reg(uint32_t reg) {
    return *(volatile uint32_t*)(e1000_dev.io_base + reg);
}

/* Write to e1000 register */
static void e1000_write_reg(uint32_t reg, uint32_t value) {
    *(volatile uint32_t*)(e1000_dev.io_base + reg) = value;
}

/* Initialize e1000 device */
int e1000_init(uint32_t pci_addr) {
    uint8_t bus = (pci_addr >> 16) & 0xFF;
    uint8_t dev = (pci_addr >> 8) & 0xFF;
    uint8_t fn = pci_addr & 0xFF;

    /* Check vendor/device ID */
    uint32_t vendor_device = pci_read32(bus, dev, fn, 0);
    uint16_t vendor_id = vendor_device & 0xFFFF;
    uint16_t device_id = vendor_device >> 16;

    if (vendor_id != 0x8086) return -1; /* Not Intel */

    int is_e1000 = 0;
    switch (device_id) {
        case 0x100E: case 0x100F: case 0x1010: case 0x1011: case 0x1012:
        case 0x1013: case 0x1014: case 0x1015: case 0x1016: case 0x1017:
        case 0x1018: case 0x1019: case 0x101A: case 0x101D: case 0x101E:
        case 0x1026: case 0x1027: case 0x1028: case 0x1075: case 0x1076:
        case 0x1077: case 0x1078: case 0x1079: case 0x107A: case 0x107B:
        case 0x107C: case 0x108A: case 0x108B: case 0x108C: case 0x1099:
            is_e1000 = 1;
            break;
    }
    if (!is_e1000) return -1;

    /* Get I/O base address */
    uint32_t bar0 = pci_read32(bus, dev, fn, 4); /* BAR0 */
    if ((bar0 & 1) == 0) {
        return -2; /* Not I/O mapped */
    }
    e1000_dev.io_base = bar0 & ~0x3;

    /* Enable bus mastering */
    pci_enable_busmaster(bus, dev, fn);

    /* Reset the device */
    e1000_write_reg(E1000_REG_CTRL, e1000_read_reg(E1000_REG_CTRL) | (1 << 26));
    /* Wait for reset to complete */
    for (volatile int i = 0; i < 100000; i++);

    /* Read MAC address from EEPROM */
    uint32_t mac_low = e1000_read_reg(E1000_REG_RAL);
    uint32_t mac_high = e1000_read_reg(E1000_REG_RAH);
    e1000_dev.mac_addr[0] = mac_low & 0xFF;
    e1000_dev.mac_addr[1] = (mac_low >> 8) & 0xFF;
    e1000_dev.mac_addr[2] = (mac_low >> 16) & 0xFF;
    e1000_dev.mac_addr[3] = (mac_low >> 24) & 0xFF;
    e1000_dev.mac_addr[4] = mac_high & 0xFF;
    e1000_dev.mac_addr[5] = (mac_high >> 8) & 0xFF;

    /* Allocate receive and transmit buffers */
    e1000_dev.rx_buffer = (uint8_t*)kmalloc(8192 + 16); /* 8KB + alignment */
    e1000_dev.tx_buffer = (uint8_t*)kmalloc(8192 + 16); /* 8KB + alignment */
    if (!e1000_dev.rx_buffer || !e1000_dev.tx_buffer) {
        return -3; /* Memory allocation failed */
    }

    /* Align buffers to 16-byte boundary */
    uintptr_t rx_addr = (uintptr_t)e1000_dev.rx_buffer;
    uintptr_t tx_addr = (uintptr_t)e1000_dev.tx_buffer;
    if (rx_addr & 0xF) rx_addr = (rx_addr + 0xF) & ~0xF;
    if (tx_addr & 0xF) tx_addr = (tx_addr + 0xF) & ~0xF;
    e1000_dev.rx_buffer = (uint8_t*)rx_addr;
    e1000_dev.tx_buffer = (uint8_t*)tx_addr;

    /* Allocate descriptor rings */
    e1000_dev.rx_descs = (e1000_rx_desc_t*)kmalloc(16 * sizeof(e1000_rx_desc_t));
    e1000_dev.tx_descs = (e1000_tx_desc_t*)kmalloc(16 * sizeof(e1000_tx_desc_t));
    if (!e1000_dev.rx_descs || !e1000_dev.tx_descs) {
        return -4; /* Memory allocation failed */
    }

    /* Initialize receive descriptors */
    for (int i = 0; i < 16; i++) {
        e1000_dev.rx_descs[i].addr = (uint64_t)(uintptr_t)(e1000_dev.rx_buffer + i * 512);
        e1000_dev.rx_descs[i].length = 0;
        e1000_dev.rx_descs[i].checksum = 0;
        e1000_dev.rx_descs[i].status = 0;
        e1000_dev.rx_descs[i].errors = 0;
        e1000_dev.rx_descs[i].special = 0;
    }

    /* Initialize transmit descriptors */
    for (int i = 0; i < 16; i++) {
        e1000_dev.tx_descs[i].addr = 0;
        e1000_dev.tx_descs[i].length = 0;
        e1000_dev.tx_descs[i].cso = 0;
        e1000_dev.tx_descs[i].cmd = 0;
        e1000_dev.tx_descs[i].status = 1; /* Descriptor done */
        e1000_dev.tx_descs[i].css = 0;
        e1000_dev.tx_descs[i].special = 0;
    }

    e1000_dev.rx_cur = 0;
    e1000_dev.tx_cur = 0;

    /* Set up receive registers */
    e1000_write_reg(E1000_REG_RDBAL, (uint32_t)(uintptr_t)e1000_dev.rx_descs);
    e1000_write_reg(E1000_REG_RDBAH, 0);
    e1000_write_reg(E1000_REG_RDRLEN, 16 * sizeof(e1000_rx_desc_t));
    e1000_write_reg(E1000_REG_RDH, 0);
    e1000_write_reg(E1000_REG_RDT, 15); /* 16 descriptors - 1 */

    /* Set up transmit registers */
    e1000_write_reg(E1000_REG_TDBAL, (uint32_t)(uintptr_t)e1000_dev.tx_descs);
    e1000_write_reg(E1000_REG_TDBAH, 0);
    e1000_write_reg(E1000_REG_TDLEN, 16 * sizeof(e1000_tx_desc_t));
    e1000_write_reg(E1000_REG_TDH, 0);
    e1000_write_reg(E1000_REG_TDT, 0);

    /* Configure receive control */
    e1000_write_reg(E1000_REG_RCTL, (1 << 1) | (1 << 3) | (1 << 4)); /* EN | BAM | BSIZE */

    /* Configure transmit control */
    e1000_write_reg(E1000_REG_TCTL, (1 << 1) | (1 << 3)); /* EN | PSP */

    return 0; /* Success */
}

/* Send packet */
void e1000_send(const void* data, uint16_t len) {
    if (len > 1518) return; /* Too large */

    /* Wait for descriptor to be available */
    while ((e1000_dev.tx_descs[e1000_dev.tx_cur].status & 1) == 0);

    /* Copy data to transmit buffer */
    uint8_t* buf = e1000_dev.tx_buffer + (e1000_dev.tx_cur * 512);
    memcpy(buf, data, len);

    /* Set up descriptor */
    e1000_dev.tx_descs[e1000_dev.tx_cur].addr = (uint64_t)(uintptr_t)buf;
    e1000_dev.tx_descs[e1000_dev.tx_cur].length = len;
    e1000_dev.tx_descs[e1000_dev.tx_cur].cmd = (1 << 0) | (1 << 3); /* EOP | IFCS */
    e1000_dev.tx_descs[e1000_dev.tx_cur].status = 0;

    /* Advance tail pointer */
    uint32_t next = e1000_dev.tx_cur + 1;
    if (next >= 16) next = 0;
    e1000_write_reg(E1000_REG_TDT, next);
    e1000_dev.tx_cur = next;
}

/* Receive packet */
int e1000_recv(void* buffer, uint16_t buffer_len) {
    /* Check if descriptor has data */
    if ((e1000_dev.rx_descs[e1000_dev.rx_cur].status & 1) == 0) {
        return 0; /* No data */
    }

    uint16_t len = e1000_dev.rx_descs[e1000_dev.rx_cur].length;
    if (len > buffer_len) len = buffer_len;

    /* Copy data */
    uint8_t* buf = e1000_dev.rx_buffer + (e1000_dev.rx_cur * 512);
    memcpy(buffer, buf, len);

    /* Clear status and advance */
    e1000_dev.rx_descs[e1000_dev.rx_cur].status = 0;
    uint32_t next = e1000_dev.rx_cur + 1;
    if (next >= 16) next = 0;
    e1000_write_reg(E1000_REG_RDT, next);
    e1000_dev.rx_cur = next;

    return len;
}