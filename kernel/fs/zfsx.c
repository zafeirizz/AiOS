#include <stdint.h>
#include "../../include/zfsx.h"
#include "../../include/ata.h"
#include "../../include/string.h"
#include "../../include/heap.h"
#include "../../include/vga.h"

/* Global filesystem state (in-memory) */
static zfsx_superblock_t sb;
static uint32_t fs_start_lba = 0;
static int fs_mounted = 0;

/* Cache for object table (simple approach: keep it all in RAM) */
/* 128 objects * 32 bytes = 4096 bytes = 1 page */
static zfsx_object_t obj_table[ZFSX_MAX_OBJECTS];

/* ── Internal Helpers ───────────────────────────────────────────── */

static void save_superblock(void) {
    ata_write(fs_start_lba + 1, 1, (uint8_t*)&sb);
}

static void save_obj_table(void) {
    /* Calculate sectors needed for object table */
    uint32_t table_size = sizeof(zfsx_object_t) * ZFSX_MAX_OBJECTS;
    uint32_t sectors = (table_size + 511) / 512;
    ata_write(fs_start_lba + sb.object_table_lba, sectors, (uint8_t*)obj_table);
}

static int alloc_block(void) {
    /* Simple log allocator: just increment the pointer */
    if (sb.next_free_lba >= sb.disk_size_blocks) return 0; // Disk full
    
    int block = sb.next_free_lba;
    sb.next_free_lba++;
    save_superblock(); /* Persist the allocation pointer */
    return block;
}

static zfsx_object_t* get_object(int id) {
    if (id < 0 || id >= ZFSX_MAX_OBJECTS) return NULL;
    return &obj_table[id];
}

/* ── Core API ───────────────────────────────────────────────────── */

int zfsx_format(uint32_t start_lba, uint32_t size_blocks) {
    /* 1. Setup Superblock */
    memset(&sb, 0, sizeof(sb));
    sb.magic = ZFSX_MAGIC;
    sb.version = ZFSX_VERSION;
    sb.block_size = ZFSX_BLOCK_SIZE;
    sb.disk_size_blocks = size_blocks;
    
    /* Layout: [Boot: 1] [SB: 1] [ObjTable: 8] [Log: ...] */
    sb.object_table_lba = 2; 
    /* 128 objects * 32 bytes = 4096 bytes = 8 sectors */
    sb.log_start_lba = 2 + 8; 
    sb.next_free_lba = sb.log_start_lba;
    
    fs_start_lba = start_lba;
    
    /* 2. Clear Object Table */
    memset(obj_table, 0, sizeof(obj_table));
    
    /* 3. Create Root Directory (ID 0) */
    zfsx_object_t* root = &obj_table[ZFSX_ROOT_ID];
    root->id = ZFSX_ROOT_ID;
    root->type = ZFSX_TYPE_DIR;
    root->size = 0;
    root->first_block = 0;
    
    /* 4. Write to disk */
    save_superblock();
    save_obj_table();
    
    terminal_writeline("[ZFSX] Format complete.");
    return 0;
}

int zfsx_mount(uint32_t start_lba) {
    fs_start_lba = start_lba;
    
    /* Read Superblock */
    ata_read(fs_start_lba + 1, 1, (uint8_t*)&sb);
    
    if (sb.magic != ZFSX_MAGIC) {
        terminal_writeline("[ZFSX] Error: Invalid magic number.");
        return -1;
    }
    
    /* Read Object Table */
    uint32_t table_size = sizeof(zfsx_object_t) * ZFSX_MAX_OBJECTS;
    uint32_t sectors = (table_size + 511) / 512;
    ata_read(fs_start_lba + sb.object_table_lba, sectors, (uint8_t*)obj_table);
    
    fs_mounted = 1;
    terminal_writeline("[ZFSX] Mounted successfully.");
    return 0;
}

int zfsx_create(int parent_dir_id, const char* name, int type) {
    if (!fs_mounted) return -1;
    
    /* 1. Find free object ID */
    int new_id = -1;
    for (int i = 1; i < ZFSX_MAX_OBJECTS; i++) {
        if (obj_table[i].type == ZFSX_TYPE_FREE) {
            new_id = i;
            break;
        }
    }
    if (new_id == -1) return -2; /* Table full */
    
    /* 2. Initialize Object */
    zfsx_object_t* obj = &obj_table[new_id];
    obj->id = new_id;
    obj->type = type;
    obj->size = 0;
    obj->first_block = 0;
    
    /* 3. Add to Parent Directory */
    /* Create a directory entry structure */
    zfsx_dirent_t entry;
    entry.obj_id = new_id;
    strncpy(entry.name, name, 60);
    
    /* Write entry to parent directory file */
    zfsx_write(parent_dir_id, &entry, sizeof(entry));
    
    /* 4. Save Table */
    save_obj_table();
    
    return new_id;
}

/* 
 * Writes data to an object.
 * In ZFSX (log-structured), we append new blocks to the object's chain.
 * For simplicity, this implementation appends to the END of the file chain.
 */
int zfsx_write(int obj_id, const void* data, uint32_t size) {
    if (!fs_mounted) return -1;
    zfsx_object_t* obj = get_object(obj_id);
    if (!obj || obj->type == ZFSX_TYPE_FREE) return -1;
    
    const uint8_t* ptr = (const uint8_t*)data;
    uint32_t bytes_left = size;
    
    /* Buffer for block I/O */
    zfsx_block_t block_buf;
    
    /* Start from the beginning to overwrite (or allocate first if empty) */
    if (obj->first_block == 0) {
        uint32_t new_blk = alloc_block();
        if (!new_blk) return -3;
        obj->first_block = new_blk;
        save_obj_table();
    }
    
    uint32_t current_lba = obj->first_block;
    uint32_t prev_lba = 0;
    
    /* Write data loop */
    while (bytes_left > 0) {
        /* Read current block to preserve next_block pointer if we are overwriting */
        ata_read(fs_start_lba + current_lba, 1, (uint8_t*)&block_buf);
        
        uint32_t next = block_buf.next_block;
        
        uint32_t chunk = (bytes_left > 508) ? 508 : bytes_left;
        memcpy(block_buf.data, ptr, chunk);
        
        /* If we need more space but hit EOF, allocate new block */
        if (bytes_left > chunk && next == 0) {
            next = alloc_block();
            if (next == 0) return -3; /* Disk full */
            block_buf.next_block = next;
        }
        
        /* Write new block to disk */
        ata_write(fs_start_lba + current_lba, 1, (uint8_t*)&block_buf);
        
        prev_lba = current_lba;
        current_lba = next;
        ptr += chunk;
        bytes_left -= chunk;
    }
    
    /* Truncate logic: if we finished writing but there are still blocks left in the chain,
     * we should ideally free them. For now, we cut the link to avoid reading old garbage. */
    if (current_lba != 0) {
        /* Read the last written block */
        ata_read(fs_start_lba + prev_lba, 1, (uint8_t*)&block_buf);
        block_buf.next_block = 0; /* Cut chain here */
        ata_write(fs_start_lba + prev_lba, 1, (uint8_t*)&block_buf);
    }
    
    obj->size = size;
    
    /* Update object metadata */
    save_obj_table();
    return size;
}

int zfsx_read(int obj_id, void* buf, uint32_t size) {
    if (!fs_mounted) return -1;
    zfsx_object_t* obj = get_object(obj_id);
    if (!obj || obj->first_block == 0) return 0;
    
    uint8_t* ptr = (uint8_t*)buf;
    uint32_t bytes_read = 0;
    uint32_t bytes_needed = size;
    if (bytes_needed > obj->size) bytes_needed = obj->size; // Clamp to file size
    
    uint32_t current_lba = obj->first_block;
    zfsx_block_t block_buf;
    
    while (bytes_read < bytes_needed && current_lba != 0) {
        ata_read(fs_start_lba + current_lba, 1, (uint8_t*)&block_buf);
        
        uint32_t chunk = 508;
        if (bytes_read + chunk > bytes_needed) {
            chunk = bytes_needed - bytes_read;
        }
        
        memcpy(ptr, block_buf.data, chunk);
        
        ptr += chunk;
        bytes_read += chunk;
        current_lba = block_buf.next_block;
    }
    
    return bytes_read;
}

void zfsx_ls(int dir_id) {
    if (!fs_mounted) return;
    zfsx_object_t* dir = get_object(dir_id);
    if (!dir || dir->type != ZFSX_TYPE_DIR) {
        terminal_writeline("Not a directory.");
        return;
    }
    
    /* Directory data is just a sequence of zfsx_dirent_t */
    uint8_t* buffer = (uint8_t*)kmalloc(dir->size);
    if (!buffer) return;
    
    zfsx_read(dir_id, buffer, dir->size);
    
    int count = dir->size / sizeof(zfsx_dirent_t);
    zfsx_dirent_t* entries = (zfsx_dirent_t*)buffer;
    
    terminal_writeline(" Type | ID  | Size   | Name");
    terminal_writeline("------+-----+--------+----------------");
    
    for (int i = 0; i < count; i++) {
        zfsx_object_t* obj = get_object(entries[i].obj_id);
        if (obj) {
            if (obj->type == ZFSX_TYPE_DIR) terminal_write(" DIR  | ");
            else                            terminal_write(" FILE | ");
            
            terminal_write_dec(obj->id);
            terminal_write(" | ");
            terminal_write_dec(obj->size);
            terminal_write(" | ");
            terminal_writeline(entries[i].name);
        }
    }
    
    kfree(buffer);
}

int zfsx_find_in_dir(int dir_id, const char* name) {
    zfsx_object_t* dir = get_object(dir_id);
    if (!dir) return -1;

    uint8_t* buffer = (uint8_t*)kmalloc(dir->size);
    if (!buffer) return -1;
    
    zfsx_read(dir_id, buffer, dir->size);
    
    int count = dir->size / sizeof(zfsx_dirent_t);
    zfsx_dirent_t* entries = (zfsx_dirent_t*)buffer;
    int result = -1;

    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, name) == 0) {
            result = entries[i].obj_id;
            break;
        }
    }
    kfree(buffer);
    return result;
}

int zfsx_resolve_path(const char* path) {
    if (!fs_mounted) return -1;
    
    int current_id = ZFSX_ROOT_ID;
    if (path[0] == '/') path++;
    if (path[0] == '\0') return current_id;

    char name[64];
    int i = 0;
    
    while (*path) {
        /* Extract next component */
        i = 0;
        while (*path && *path != '/' && i < 63) {
            name[i++] = *path++;
        }
        name[i] = '\0';
        if (*path == '/') path++;
        
        if (name[0] == '\0') continue;
        
        /* Find component in current dir */
        int next_id = zfsx_find_in_dir(current_id, name);
        if (next_id < 0) return -1;
        
        /* Check if it is a directory (unless it's the last component) */
        zfsx_object_t* obj = get_object(next_id);
        if (obj->type != ZFSX_TYPE_DIR && *path != '\0') return -1;
        
        current_id = next_id;
    }
    
    return current_id;
}

void zfsx_stat(void) {
    if (!fs_mounted) {
        terminal_writeline("ZFSX not mounted.");
        return;
    }
    terminal_writeline("=== ZFSX Statistics ===");
    terminal_write("Superblock LBA: "); terminal_write_dec(fs_start_lba + 1); terminal_writeline("");
    terminal_write("Log Start LBA:  "); terminal_write_dec(fs_start_lba + sb.log_start_lba); terminal_writeline("");
    terminal_write("Next Free LBA:  "); terminal_write_dec(fs_start_lba + sb.next_free_lba); terminal_writeline("");
    terminal_write("Total Blocks:   "); terminal_write_dec(sb.disk_size_blocks); terminal_writeline("");
    
    int used_objs = 0;
    for(int i=0; i<ZFSX_MAX_OBJECTS; i++) if(obj_table[i].type != ZFSX_TYPE_FREE) used_objs++;
    
    terminal_write("Objects Used:   "); terminal_write_dec(used_objs); 
    terminal_write("/"); terminal_write_dec(ZFSX_MAX_OBJECTS); terminal_writeline("");
}

int zfsx_delete(int obj_id) {
    if (!fs_mounted) return -1;
    zfsx_object_t* obj = get_object(obj_id);
    if (!obj || obj->type == ZFSX_TYPE_FREE) return -1;
    obj->type = ZFSX_TYPE_FREE;
    save_obj_table();
    return 0;
}

/* ── New functions for fileman integration ──────────────────────── */

int zfsx_is_mounted(void) {
    return fs_mounted;
}

void zfsx_list_root(void (*cb)(const char* name, uint32_t size)) {
    if (!fs_mounted) return;
    zfsx_ls(ZFSX_ROOT_ID);
    /* But zfsx_ls prints to terminal, not callback. Need to adapt. */
    /* For now, since fileman expects callback, let's modify zfsx_ls to use callback if provided. */
    /* But to keep simple, perhaps duplicate the logic. */
    zfsx_object_t* dir = get_object(ZFSX_ROOT_ID);
    if (!dir || dir->type != ZFSX_TYPE_DIR) return;

    uint8_t* buffer = (uint8_t*)kmalloc(dir->size);
    if (!buffer) return;
    
    zfsx_read(ZFSX_ROOT_ID, buffer, dir->size);
    
    int count = dir->size / sizeof(zfsx_dirent_t);
    zfsx_dirent_t* entries = (zfsx_dirent_t*)buffer;
    
    for (int i = 0; i < count; i++) {
        zfsx_object_t* obj = get_object(entries[i].obj_id);
        if (obj) {
            cb(entries[i].name, obj->size);
        }
    }
    
    kfree(buffer);
}

int zfsx_read_file(const char* path, uint8_t* buf, uint32_t buf_size) {
    if (!fs_mounted) return -1;
    
    /* Simple: assume path is just filename in root */
    int obj_id = zfsx_find_in_dir(ZFSX_ROOT_ID, path);
    if (obj_id < 0) return -1;
    
    zfsx_object_t* obj = get_object(obj_id);
    if (!obj || obj->type != ZFSX_TYPE_FILE) return -1;
    
    if (obj->size > buf_size) return -1; /* Buffer too small */
    
    return zfsx_read(obj_id, buf, obj->size);
}