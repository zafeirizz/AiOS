#include <stdint.h>
#include "../../include/rtl8139.h"
#include "../../include/pci.h"
#include "../../include/irq.h"
#include "../../include/string.h"
#include "../../include/vga.h"

/* ── I/O helpers ──────────────────────────────────────── */
static inline void     outb(uint16_t p, uint8_t  v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline void     outw(uint16_t p, uint16_t v) { __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline void     outl(uint16_t p, uint32_t v) { __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t  inb (uint16_t p) { uint8_t  v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline uint16_t inw (uint16_t p) { uint16_t v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline uint32_t inl (uint16_t p) { uint32_t v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p)); return v; }

/* ── RTL8139 register offsets ─────────────────────────── */
#define RTL_MAC0        0x00
#define RTL_MAR0        0x08   /* Multicast filter           */
#define RTL_TXSTATUS0   0x10   /* TX status descriptors 0-3  */
#define RTL_TXADDR0     0x20   /* TX buffer addresses 0-3    */
#define RTL_RXBUF       0x30   /* RX ring buffer address     */
#define RTL_CMD         0x37
#define RTL_CAPR        0x38   /* Current Address of Packet Read */
#define RTL_IMR         0x3C   /* Interrupt mask             */
#define RTL_ISR         0x3E   /* Interrupt status           */
#define RTL_TCR         0x40   /* TX config                  */
#define RTL_RCR         0x44   /* RX config                  */
#define RTL_CONFIG1     0x52

/* CMD bits */
#define CMD_RST         0x10
#define CMD_RE          0x08   /* Receiver enable            */
#define CMD_TE          0x04   /* Transmitter enable         */
#define CMD_BUFE        0x01   /* Buffer empty               */

/* ISR/IMR bits */
#define INT_ROK         0x0001 /* RX OK                      */
#define INT_RER         0x0002 /* RX error                   */
#define INT_TOK         0x0004 /* TX OK                      */
#define INT_TER         0x0008 /* TX error                   */
#define INT_RXOVW       0x0010 /* RX buffer overflow         */

/* RCR bits */
#define RCR_AAP         (1<<0) /* Accept all packets         */
#define RCR_APM         (1<<1) /* Accept physical match      */
#define RCR_AM          (1<<2) /* Accept multicast           */
#define RCR_AB          (1<<3) /* Accept broadcast           */
#define RCR_WRAP        (1<<7) /* Wrap at end of buffer      */
#define RCR_MXDMA_UNLIM (7<<8)
#define RCR_RBLEN_32K   (2<<11)

/* ── RX ring buffer (32 KB + 16 bytes for wrap-around) ── */
#define RX_BUF_SIZE  (32*1024 + 16)
#define TX_BUF_SIZE  2048
#define TX_DESCS     4

static uint8_t  rx_buf[RX_BUF_SIZE] __attribute__((aligned(4)));
static uint8_t  tx_buf[TX_DESCS][TX_BUF_SIZE] __attribute__((aligned(4)));

static uint16_t rx_ptr  = 0;   /* current read offset in rx_buf */
static uint8_t  tx_slot = 0;   /* next TX descriptor to use     */
static uint16_t io_base = 0;
static uint8_t  mac[6];
static int      ready   = 0;

/* ── IRQ handler ──────────────────────────────────────── */
void rtl8139_irq_handler(void) {
    uint16_t isr = inw(io_base + RTL_ISR);
    outw(io_base + RTL_ISR, isr);   /* ACK all */
}

/* ── Init ─────────────────────────────────────────────── */
int rtl8139_init(void) {
    uint8_t bus, dev;
    if (!pci_find_device(RTL_VENDOR, RTL_DEVICE, &bus, &dev)) {
        terminal_writeline("RTL8139: not found on PCI bus");
        return -1;
    }

    /* Enable PCI bus-mastering and I/O space */
    pci_enable_busmaster(bus, dev, 0);
    io_base = (uint16_t)pci_bar0_io(bus, dev, 0);

    terminal_write("RTL8139: found at PCI ");
    terminal_write_dec(bus); terminal_write(":"); terminal_write_dec(dev);
    terminal_write("  I/O base 0x"); terminal_write_hex(io_base);
    terminal_putchar('\n');

    /* Power on */
    outb(io_base + RTL_CONFIG1, 0x00);

    /* Software reset */
    outb(io_base + RTL_CMD, CMD_RST);
    for (int i = 0; i < 100000; i++)
        if (!(inb(io_base + RTL_CMD) & CMD_RST)) break;

    /* Read MAC address */
    for (int i = 0; i < 6; i++)
        mac[i] = inb(io_base + RTL_MAC0 + i);

    terminal_write("RTL8139: MAC ");
    for (int i = 0; i < 6; i++) {
        terminal_write_hex(mac[i]);
        if (i < 5) terminal_putchar(':');
    }
    terminal_putchar('\n');

    /* Set RX buffer address */
    outl(io_base + RTL_RXBUF, (uint32_t)(uintptr_t)rx_buf);

    /* Accept broadcast + physical match, no wrap, 32K buffer */
    outl(io_base + RTL_RCR,
         RCR_AB | RCR_APM | RCR_AM |
         RCR_MXDMA_UNLIM | RCR_RBLEN_32K);

    /* TX config: max DMA burst, no loopback */
    outl(io_base + RTL_TCR, (6<<8));

    /* Enable RX + TX */
    outb(io_base + RTL_CMD, CMD_RE | CMD_TE);

    /* Enable RX-OK and TX-OK interrupts */
    outw(io_base + RTL_IMR, INT_ROK | INT_TOK | INT_RER | INT_TER);

    /* Get IRQ line from PCI and register handler */
    uint8_t irq_line = pci_read8(bus, dev, 0, 0x3C);
    irq_register_handler(irq_line, rtl8139_irq_handler);
    irq_clear_mask(irq_line);

    terminal_write("RTL8139: IRQ "); terminal_write_dec(irq_line);
    terminal_writeline(" — ready");

    rx_ptr = 0;
    ready  = 1;
    return 0;
}

/* ── Send ─────────────────────────────────────────────── */
void rtl8139_send(const void* data, uint16_t len) {
    if (!ready) return;
    if (len > TX_BUF_SIZE) return;

    uint8_t slot = tx_slot & 3;
    memcpy(tx_buf[slot], data, len);

    /* Write buffer address */
    outl(io_base + RTL_TXADDR0 + slot*4, (uint32_t)(uintptr_t)tx_buf[slot]);
    /* Write size + start transmission (bit 13 = OWN) */
    outl(io_base + RTL_TXSTATUS0 + slot*4, len & 0x1FFF);

    /* Wait for TOK */
    for (int i = 0; i < 100000; i++) {
        if (inl(io_base + RTL_TXSTATUS0 + slot*4) & (1<<15)) break;
    }

    tx_slot = (tx_slot + 1) & 3;
}

/* ── Receive ──────────────────────────────────────────── */
uint16_t rtl8139_recv(void* buf, uint16_t buf_len) {
    if (!ready) return 0;

    /* Buffer empty? */
    if (inb(io_base + RTL_CMD) & CMD_BUFE) return 0;

    /* The RTL8139 prepends a 4-byte header: status(2) + length(2) */
    uint16_t offset = rx_ptr % RX_BUF_SIZE;
    uint16_t status = *(uint16_t*)(rx_buf + offset);
    uint16_t pkt_len = *(uint16_t*)(rx_buf + offset + 2);

    if (!(status & 0x0001)) return 0;   /* ROK not set */

    /* pkt_len includes the 4-byte CRC appended by the NIC */
    uint16_t data_len = pkt_len - 4;
    if (data_len > buf_len) data_len = buf_len;

    uint16_t data_off = (offset + 4) % RX_BUF_SIZE;

    /* Handle wrap-around in ring buffer */
    if (data_off + data_len <= RX_BUF_SIZE) {
        memcpy(buf, rx_buf + data_off, data_len);
    } else {
        uint16_t first = RX_BUF_SIZE - data_off;
        memcpy(buf, rx_buf + data_off, first);
        memcpy((uint8_t*)buf + first, rx_buf, data_len - first);
    }

    /* Advance read pointer (DWORD-aligned, +4 for header) */
    rx_ptr = (uint16_t)((rx_ptr + pkt_len + 4 + 3) & ~3u);
    /* Tell NIC new CAPR (subtract 0x10 per datasheet) */
    outw(io_base + RTL_CAPR, rx_ptr - 0x10);

    return data_len;
}

const uint8_t* rtl8139_mac(void) { return mac; }
