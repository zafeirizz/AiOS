#ifndef DISK_H
#define DISK_H

#include <stdint.h>

/* Generic disk driver abstraction */

/* Read a block from disk
 * lba: logical block address
 * buffer: destination (assume 512 bytes or disk block size)
 * returns: 0 on success, -1 on error
 */
int disk_read(uint32_t lba, void* buffer);

/* Write a block to disk
 * lba: logical block address
 * buffer: source (assume 512 bytes or disk block size)
 * returns: 0 on success, -1 on error
 */
int disk_write(uint32_t lba, void* buffer);

/* Get disk block size in bytes */
uint32_t disk_get_block_size(void);

/* Get total disk size in blocks */
uint32_t disk_get_total_blocks(void);

#endif
