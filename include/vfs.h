#ifndef VFS_H
#define VFS_H

#include <stdint.h>

/* Virtual File System abstraction layer
 * 
 * Allows multiple filesystems to coexist
 * Apps use unified API regardless of underlying FS
 */

#define VFS_MAX_MOUNTS 8

/* File descriptor */
typedef struct {
    int mount_id;     /* which mount this belongs to */
    uint32_t inode;   /* inode/object ID in that FS */
    uint32_t pos;     /* current position in file */
} vfs_fd_t;

/* Filesystem operations (plugin interface) */
typedef struct {
    int (*format)(uint32_t blocks);
    int (*mount)(void);
    int (*umount)(void);
    int (*open)(const char* path);  /* returns inode ID */
    int (*create)(const char* path, int type);
    int (*read)(uint32_t inode, void* buf, uint32_t size);
    int (*write)(uint32_t inode, const void* buf, uint32_t size);
    int (*delete)(uint32_t inode);
    int (*list)(uint32_t dir_inode, void (*callback)(const char*, uint32_t));
} vfs_filesystem_t;

/* Mount point */
typedef struct {
    char path[64];
    int active;
    vfs_filesystem_t* fs;
} vfs_mount_t;

/* === VFS API === */

/* Mount a filesystem at a path
 * path: mount point (e.g., "/", "/mnt/fat")
 * fs: filesystem operations
 * returns: mount ID, or -1 on failure
 */
int vfs_mount(const char* path, vfs_filesystem_t* fs);

/* Unmount a filesystem */
int vfs_umount(int mount_id);

/* Open a file by path (unified across mounts)
 * path: absolute path (e.g., "/home/file.txt")
 * returns: file descriptor, or -1 on error
 */
int vfs_open(const char* path);

/* Create a file or directory
 * path: absolute path
 * type: 1 = file, 2 = directory
 * returns: 0 on success, -1 on error
 */
int vfs_create(const char* path, int type);

/* Read from open file */
int vfs_read(int fd, void* buffer, uint32_t size);

/* Write to open file */
int vfs_write(int fd, const void* buffer, uint32_t size);

/* Close file descriptor */
int vfs_close(int fd);

/* Seek to position in file */
int vfs_seek(int fd, uint32_t position);

/* Delete file */
int vfs_delete(const char* path);

/* List directory */
void vfs_list(const char* path, void (*callback)(const char*, uint32_t));

/* Get current working directory */
const char* vfs_getcwd(void);

/* Change current working directory */
int vfs_chdir(const char* path);

#endif
