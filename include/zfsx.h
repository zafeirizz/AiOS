#ifndef ZFSX_H
#define ZFSX_H

#include <stdint.h>

#define ZFSX_MAGIC       0x5A465358  /* "ZFSX" in hex */
#define ZFSX_VERSION     1
#define ZFSX_BLOCK_SIZE  512
#define ZFSX_MAX_OBJECTS 128         /* Fixed object table size for simplicity */

#define ZFSX_TYPE_FREE   0
#define ZFSX_TYPE_FILE   1
#define ZFSX_TYPE_DIR    2

#define ZFSX_ROOT_ID     0

/* 1. Superblock (LBA 1) */
typedef struct {
    uint32_t magic;              /* ZFSX_MAGIC */
    uint32_t version;
    uint32_t block_size;
    uint32_t disk_size_blocks;
    uint32_t object_table_lba;   /* Start of object table */
    uint32_t log_start_lba;      /* Start of data area */
    uint32_t next_free_lba;      /* Log head pointer (allocator) */
    uint8_t  padding[484];
} __attribute__((packed)) zfsx_superblock_t;

/* 2. Object Entry (32 bytes) */
typedef struct {
    uint32_t id;
    uint32_t type;               /* FILE or DIR */
    uint32_t size;               /* Total size in bytes */
    uint32_t first_block;        /* LBA of first data block */
    uint32_t created_time;       /* Placeholder */
    uint32_t modified_time;      /* Placeholder */
    uint32_t reserved[2];
} __attribute__((packed)) zfsx_object_t;

/* 3. Data Block Header (Linked List in Log) */
/* Each 512-byte sector contains this header + data */
typedef struct {
    uint32_t next_block;         /* LBA of next block, or 0 if EOF */
    uint8_t  data[508];          /* Actual file content */
} __attribute__((packed)) zfsx_block_t;

/* 4. Directory Entry */
typedef struct {
    uint32_t obj_id;
    char     name[60];
} __attribute__((packed)) zfsx_dirent_t;

/* ── API ────────────────────────────────────────────────────────── */

/* Format the disk (LBA start, size in blocks) */
int zfsx_format(uint32_t start_lba, uint32_t size_blocks);

/* Mount the filesystem */
int zfsx_mount(uint32_t start_lba);

/* Create a new file or directory inside a parent dir */
int zfsx_create(int parent_dir_id, const char* name, int type);

/* Write data to a file object (append/overwrite logic) */
int zfsx_write(int obj_id, const void* data, uint32_t size);

/* Read data from a file object */
int zfsx_read(int obj_id, void* buf, uint32_t size);

/* List directory contents */
void zfsx_ls(int dir_id);

/* Utilities */
int zfsx_delete(int obj_id);
int zfsx_find_in_dir(int dir_id, const char* name);
void zfsx_stat(void);
int zfsx_resolve_path(const char* path);

/* New functions for fileman */
int zfsx_is_mounted(void);
void zfsx_list_root(void (*cb)(const char* name, uint32_t size));
int zfsx_read_file(const char* path, uint8_t* buf, uint32_t buf_size);

#endif