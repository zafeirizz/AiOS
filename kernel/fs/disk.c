#include <stdint.h>
#include "../../include/disk.h"
#include "../../include/ata.h"

#define DISK_BLOCK_SIZE 512

int disk_read(uint32_t lba, void* buffer) {
    if (!buffer) return -1;
    
    /* Use ATA driver to read sector(s) */
    /* ata_read expects: start LBA, count, buffer */
    return ata_read(lba, 1, (uint8_t*)buffer) == 0 ? 0 : -1;
}

int disk_write(uint32_t lba, void* buffer) {
    if (!buffer) return -1;
    
    /* Use ATA driver to write sector(s) */
    return ata_write(lba, 1, (uint8_t*)buffer) == 0 ? 0 : -1;
}

uint32_t disk_get_block_size(void) {
    return DISK_BLOCK_SIZE;
}

uint32_t disk_get_total_blocks(void) {
    /* For now, assume ~100MB virtual disk in QEMU = ~204800 sectors */
    /* TODO: query disk geometry from ATA driver */
    return 204800;
}
