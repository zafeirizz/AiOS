#include <stdint.h>
#include "../../include/ata.h"
#include "../../include/string.h"

/* ── I/O helpers ──────────────────────────────────────── */

static inline void outb(uint16_t p, uint8_t v)  { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline void outw(uint16_t p, uint16_t v) { __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t  inb(uint16_t p)  { uint8_t  v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline uint16_t inw(uint16_t p)  { uint16_t v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p)); return v; }

/* ── Primary ATA bus registers (master drive) ─────────── */

#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_DRIVE_SEL   0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7
#define ATA_ALT_STATUS  0x3F6

/* Status register bits */
#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

/* Commands */
#define ATA_CMD_READ_PIO  0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_FLUSH     0xE7
#define ATA_CMD_IDENTIFY  0xEC

/* ── 400ns delay (read alt status 4 times) ────────────── */

static void ata_delay(void) {
    inb(ATA_ALT_STATUS); inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS); inb(ATA_ALT_STATUS);
}

/* ── Poll until BSY clears, return 0 or -1 on error ──── */

static int ata_poll(void) {
    ata_delay();
    for (uint32_t i = 0; i < 100000; i++) {
        uint8_t st = inb(ATA_STATUS);
        if (st & ATA_SR_ERR)  return -1;
        if (!(st & ATA_SR_BSY) && (st & ATA_SR_DRQ)) return 0;
    }
    return -1;   /* timeout */
}

static int ata_wait_ready(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        uint8_t st = inb(ATA_STATUS);
        if (st & ATA_SR_ERR)  return -1;
        if (!(st & ATA_SR_BSY) && (st & ATA_SR_DRDY)) return 0;
    }
    return -1;
}

/* ── Setup LBA28 registers ────────────────────────────── */

static void ata_setup_lba(uint32_t lba, uint8_t count) {
    outb(ATA_DRIVE_SEL,  0xE0 | ((lba >> 24) & 0x0F));  /* LBA mode, master */
    ata_delay();
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LO,     (uint8_t)(lba      ));
    outb(ATA_LBA_MID,    (uint8_t)(lba >>  8));
    outb(ATA_LBA_HI,     (uint8_t)(lba >> 16));
}

/* ── Public API ───────────────────────────────────────── */

int ata_init(void) {
    /* Select master drive on primary bus */
    outb(ATA_DRIVE_SEL, 0xA0);
    ata_delay();

    /* Send IDENTIFY */
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay();

    uint8_t st = inb(ATA_STATUS);
    if (st == 0) return 0;   /* no drive */

    /* Wait for BSY to clear */
    for (uint32_t i = 0; i < 100000; i++) {
        st = inb(ATA_STATUS);
        if (!(st & ATA_SR_BSY)) break;
    }

    /* Check for ATAPI (non-ATA) */
    if (inb(ATA_LBA_MID) || inb(ATA_LBA_HI)) return 0;

    /* Wait for DRQ or ERR */
    for (uint32_t i = 0; i < 100000; i++) {
        st = inb(ATA_STATUS);
        if (st & ATA_SR_ERR) return 0;
        if (st & ATA_SR_DRQ) break;
    }

    /* Drain the 256-word IDENTIFY buffer */
    for (int i = 0; i < 256; i++) inw(ATA_DATA);

    return 1;   /* drive present */
}

int ata_read(uint32_t lba, uint32_t count, uint8_t* buf) {
    while (count > 0) {
        uint8_t n = (count > 255) ? 255 : (uint8_t)count;

        if (ata_wait_ready() < 0) return -1;
        ata_setup_lba(lba, n);
        outb(ATA_COMMAND, ATA_CMD_READ_PIO);

        for (uint8_t s = 0; s < n; s++) {
            if (ata_poll() < 0) return -1;
            /* Read 256 words (512 bytes) */
            for (int i = 0; i < 256; i++) {
                uint16_t w = inw(ATA_DATA);
                buf[0] = (uint8_t)(w      );
                buf[1] = (uint8_t)(w >> 8);
                buf += 2;
            }
        }
        lba   += n;
        count -= n;
    }
    return 0;
}

int ata_write(uint32_t lba, uint32_t count, const uint8_t* buf) {
    while (count > 0) {
        uint8_t n = (count > 255) ? 255 : (uint8_t)count;

        if (ata_wait_ready() < 0) return -1;
        ata_setup_lba(lba, n);
        outb(ATA_COMMAND, ATA_CMD_WRITE_PIO);

        for (uint8_t s = 0; s < n; s++) {
            if (ata_poll() < 0) return -1;
            for (int i = 0; i < 256; i++) {
                uint16_t w = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
                outw(ATA_DATA, w);
                buf += 2;
            }
        }
        lba   += n;
        count -= n;
    }
    /* Flush cache */
    outb(ATA_COMMAND, ATA_CMD_FLUSH);
    ata_wait_ready();
    return 0;
}
