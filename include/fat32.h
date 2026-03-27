#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

/*
 * fat32_mount_mbr() — recommended entry point.
 * Reads the MBR at LBA 0, finds the first FAT32 partition
 * (type 0x0B or 0x0C), and mounts it automatically.
 * Returns 0 on success, -1 on failure.
 */
int fat32_mount_mbr(void);

/*
 * fat32_mount() — mount directly at a known LBA.
 * Use this if you already know the partition start LBA,
 * or if the disk has no MBR (e.g. a raw FAT32 image).
 * Returns 0 on success, -1 on failure.
 */
int fat32_mount(uint32_t lba_start);

/* Print info about the mounted volume to the terminal. */
void fat32_info(void);

/*
 * List files in a directory path (e.g. "/" or "/DOCS").
 * Calls callback(name, size, is_dir) for each entry found.
 * Pass "/" or "" for the root directory.
 */
void fat32_list(void (*cb)(const char* name, uint32_t size));

/*
 * Read a file into buf.
 * path: full path like "/README.TXT" or just "README.TXT" for root.
 * Returns bytes read, or -1 if not found / error.
 */
int fat32_read_file(const char* path, uint8_t* buf, uint32_t buf_size);

/* Delete a file. */
int fat32_delete(const char* path);

/* Check if FAT32 is mounted */
int fat32_is_mounted(void);

#endif
