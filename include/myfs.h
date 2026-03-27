#ifndef MYFS_H
#define MYFS_H

#include <stdint.h>

#define MYFS_MAX_NAME    32
#define MYFS_MAX_FILES   64
#define MYFS_BLOCK_SIZE  512
#define MYFS_MAGIC       0x4D594653   /* "MYFS" */

/* Format a region of disk starting at `lba_start` with `total_blocks` blocks.
 * This DESTROYS any existing data. Returns 0 on success. */
int myfs_format(uint32_t lba_start, uint32_t total_blocks);

/* Mount an existing MyFS volume at `lba_start`.
 * Returns 0 on success, -1 if magic doesn't match. */
int myfs_mount(uint32_t lba_start);

/* Check if MyFS is mounted. */
int myfs_is_mounted(void);

/* Write data to a file (creates or overwrites).
 * Returns bytes written, or -1 on error. */
int myfs_write(const char* name, const uint8_t* data, uint32_t size);

/* Read a file into buf.  Returns bytes read, or -1 if not found. */
int myfs_read(const char* name, uint8_t* buf, uint32_t buf_size);

/* Delete a file. Returns 0 on success, -1 if not found. */
int myfs_delete(const char* name);

/* List files. Calls callback(name, size) for each. */
void myfs_list(void (*callback)(const char* name, uint32_t size));

#endif
