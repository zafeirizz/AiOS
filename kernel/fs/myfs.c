#include <stdint.h>
#include "../../include/myfs.h"
#include "../../include/ata.h"
#include "../../include/string.h"

/*
 * MyFS disk layout
 * ─────────────────────────────────────────────
 * LBA+0          Superblock      (1 sector)
 * LBA+1 ..+4     File table      (4 sectors = 64 entries × 32 bytes)
 * LBA+5 ..+N     Data blocks     (rest of the volume)
 * ─────────────────────────────────────────────
 *
 * Each file entry stores: name, start_block (relative to data area), size.
 * Files are stored contiguously — no fragmentation tracking yet.
 * Free space is found by scanning for the highest used block.
 */

#define SUPERBLOCK_LBA_OFF  0
#define FILETABLE_LBA_OFF   1
#define FILETABLE_SECTORS   4
#define DATA_LBA_OFF        (FILETABLE_LBA_OFF + FILETABLE_SECTORS)  /* = 5 */

#define ENTRIES_PER_SECTOR  (ATA_SECTOR_SIZE / sizeof(myfs_file_entry_t))
#define MAX_ENTRIES         (FILETABLE_SECTORS * (int)ENTRIES_PER_SECTOR)

/* ── On-disk structures ───────────────────────────────── */

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t data_blocks;      /* total_blocks - DATA_LBA_OFF */
    uint32_t reserved[5];
} __attribute__((packed)) myfs_superblock_t;

typedef struct {
    char     name[MYFS_MAX_NAME];
    uint32_t start_block;      /* offset into data area */
    uint32_t size;             /* bytes */
    uint8_t  used;             /* 1 = valid entry */
    uint8_t  pad[3];
} __attribute__((packed)) myfs_file_entry_t;

/* ── Mount state ──────────────────────────────────────── */

static uint32_t vol_lba;
static uint32_t data_blocks;
static int      mounted = 0;

static uint8_t          sector_buf[ATA_SECTOR_SIZE];
static myfs_superblock_t sb;

/* ── Helpers ──────────────────────────────────────────── */

static int read_entry(int idx, myfs_file_entry_t* e) {
    int sector = idx / (int)ENTRIES_PER_SECTOR;
    int offset = idx % (int)ENTRIES_PER_SECTOR;
    if (ata_read(vol_lba + FILETABLE_LBA_OFF + sector, 1, sector_buf) < 0) return -1;
    *e = ((myfs_file_entry_t*)sector_buf)[offset];
    return 0;
}

static int write_entry(int idx, const myfs_file_entry_t* e) {
    int sector = idx / (int)ENTRIES_PER_SECTOR;
    int offset = idx % (int)ENTRIES_PER_SECTOR;
    if (ata_read(vol_lba + FILETABLE_LBA_OFF + sector, 1, sector_buf) < 0) return -1;
    ((myfs_file_entry_t*)sector_buf)[offset] = *e;
    if (ata_write(vol_lba + FILETABLE_LBA_OFF + sector, 1, sector_buf) < 0) return -1;
    return 0;
}

/* Find entry by name. Returns index or -1. */
static int find_entry(const char* name) {
    myfs_file_entry_t e;
    for (int i = 0; i < MAX_ENTRIES; i++) {
        if (read_entry(i, &e) < 0) return -1;
        if (e.used && strcmp(e.name, name) == 0) return i;
    }
    return -1;
}

/* Find a free entry slot. Returns index or -1. */
static int find_free_entry(void) {
    myfs_file_entry_t e;
    for (int i = 0; i < MAX_ENTRIES; i++) {
        if (read_entry(i, &e) < 0) return -1;
        if (!e.used) return i;
    }
    return -1;
}

/* Find first free data block (after last used block). */
static uint32_t find_free_data_block(void) {
    uint32_t next = 0;
    myfs_file_entry_t e;
    for (int i = 0; i < MAX_ENTRIES; i++) {
        if (read_entry(i, &e) < 0) break;
        if (!e.used) continue;
        uint32_t end = e.start_block + (e.size + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE;
        if (end > next) next = end;
    }
    return next;
}

/* ── Public API ───────────────────────────────────────── */

int myfs_format(uint32_t lba_start, uint32_t total_blocks) {
    vol_lba = lba_start;

    /* Write superblock */
    memset(sector_buf, 0, ATA_SECTOR_SIZE);
    myfs_superblock_t* s = (myfs_superblock_t*)sector_buf;
    s->magic        = MYFS_MAGIC;
    s->total_blocks = total_blocks;
    s->data_blocks  = total_blocks - DATA_LBA_OFF;
    if (ata_write(lba_start + SUPERBLOCK_LBA_OFF, 1, sector_buf) < 0) return -1;

    /* Zero out file table */
    memset(sector_buf, 0, ATA_SECTOR_SIZE);
    for (int i = 0; i < FILETABLE_SECTORS; i++) {
        if (ata_write(lba_start + FILETABLE_LBA_OFF + i, 1, sector_buf) < 0) return -1;
    }

    mounted    = 1;
    data_blocks = total_blocks - DATA_LBA_OFF;
    return 0;
}

int myfs_mount(uint32_t lba_start) {
    vol_lba = lba_start;
    if (ata_read(lba_start + SUPERBLOCK_LBA_OFF, 1, sector_buf) < 0) return -1;
    memcpy(&sb, sector_buf, sizeof(sb));
    if (sb.magic != MYFS_MAGIC) return -1;
    data_blocks = sb.data_blocks;
    mounted = 1;
    return 0;
}

int myfs_is_mounted(void) {
    return mounted;
}

int myfs_write(const char* name, const uint8_t* data, uint32_t size) {
    if (!mounted) return -1;

    /* Delete existing entry with the same name if present */
    myfs_delete(name);

    int slot = find_free_entry();
    if (slot < 0) return -1;

    uint32_t start = find_free_data_block();
    uint32_t sectors_needed = (size + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE;

    if (start + sectors_needed > data_blocks) return -1;  /* disk full */

    /* Write data sectors */
    uint32_t written = 0;
    for (uint32_t s = 0; s < sectors_needed; s++) {
        memset(sector_buf, 0, ATA_SECTOR_SIZE);
        uint32_t chunk = size - written;
        if (chunk > ATA_SECTOR_SIZE) chunk = ATA_SECTOR_SIZE;
        memcpy(sector_buf, data + written, chunk);
        if (ata_write(vol_lba + DATA_LBA_OFF + start + s, 1, sector_buf) < 0) return -1;
        written += chunk;
    }

    /* Write file table entry */
    myfs_file_entry_t e;
    memset(&e, 0, sizeof(e));
    strncpy(e.name, name, MYFS_MAX_NAME - 1);
    e.start_block = start;
    e.size        = size;
    e.used        = 1;
    if (write_entry(slot, &e) < 0) return -1;

    return (int)written;
}

int myfs_read(const char* name, uint8_t* buf, uint32_t buf_size) {
    if (!mounted) return -1;

    int idx = find_entry(name);
    if (idx < 0) return -1;

    myfs_file_entry_t e;
    if (read_entry(idx, &e) < 0) return -1;

    uint32_t sectors = (e.size + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE;
    uint32_t written = 0;

    for (uint32_t s = 0; s < sectors; s++) {
        if (ata_read(vol_lba + DATA_LBA_OFF + e.start_block + s, 1, sector_buf) < 0) return -1;
        uint32_t chunk = e.size - written;
        if (chunk > ATA_SECTOR_SIZE) chunk = ATA_SECTOR_SIZE;
        if (written + chunk > buf_size) chunk = buf_size - written;
        memcpy(buf + written, sector_buf, chunk);
        written += chunk;
    }
    return (int)written;
}

int myfs_delete(const char* name) {
    if (!mounted) return -1;
    int idx = find_entry(name);
    if (idx < 0) return -1;
    myfs_file_entry_t e;
    read_entry(idx, &e);
    e.used = 0;
    return write_entry(idx, &e);
}

void myfs_list(void (*cb)(const char* name, uint32_t size)) {
    if (!mounted) return;
    myfs_file_entry_t e;
    for (int i = 0; i < MAX_ENTRIES; i++) {
        if (read_entry(i, &e) < 0) break;
        if (e.used) cb(e.name, e.size);
    }
}
