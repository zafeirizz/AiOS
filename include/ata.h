#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_SECTOR_SIZE 512

/*
 * Initialise the primary ATA bus and detect drives.
 * Returns 1 if a drive is present, 0 if not.
 */
int ata_init(void);

/* Read `count` 512-byte sectors starting at LBA28 address `lba`
 * into `buf`.  Returns 0 on success, -1 on error. */
int ata_read(uint32_t lba, uint32_t count, uint8_t* buf);

/* Write `count` sectors from `buf` to disk starting at `lba`.
 * Returns 0 on success, -1 on error. */
int ata_write(uint32_t lba, uint32_t count, const uint8_t* buf);

#endif
