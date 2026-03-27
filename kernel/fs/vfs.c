#include <stdint.h>
#include <string.h>
#include "../../include/vfs.h"
#include "../../include/heap.h"
#include "../../include/vga.h"

#define VFS_MAX_FDS 256

/* Mount point registry */
static vfs_mount_t mounts[VFS_MAX_MOUNTS];
static int mount_count = 0;

/* File descriptor table */
static vfs_fd_t fd_table[VFS_MAX_FDS];
static int fd_count = 0;

/* Current working directory */
static char cwd[256] = "/";

/* ── Helper: Parse path and find mount ── */
static void append_string(char* dest, const char* src) {
    while (*dest) dest++;
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

static vfs_mount_t* find_mount_for_path(const char* path) {
    if (!path || path[0] != '/') return NULL;
    
    /* Try longest mount first */
    vfs_mount_t* best = &mounts[0];  /* root is default */
    int best_len = 1;
    
    for (int i = 0; i < mount_count; i++) {
        if (!mounts[i].active) continue;
        int mlen = strlen(mounts[i].path);
        if (mlen > best_len && strncmp(path, mounts[i].path, mlen) == 0) {
            best = &mounts[i];
            best_len = mlen;
        }
    }
    
    return best->active ? best : NULL;
}

/* ── Helper: Convert absolute path to relative (within mount) ── */
static void get_relative_path(const char* abs_path, vfs_mount_t* mount, char* rel_path) {
    int mount_len = strlen(mount->path);
    if (mount_len == 1) {
        strcpy(rel_path, abs_path);
    } else {
        strcpy(rel_path, abs_path + mount_len);
        if (rel_path[0] != '/') {
            char tmp[256];
            strcpy(tmp, "/");
            append_string(tmp, rel_path);
            strcpy(rel_path, tmp);
        }
    }
}

/* ── VFS API Implementation ── */

int vfs_mount(const char* path, vfs_filesystem_t* fs) {
    if (mount_count >= VFS_MAX_MOUNTS || !path || !fs) return -1;
    
    vfs_mount_t* mount = &mounts[mount_count];
    strncpy(mount->path, path, sizeof(mount->path) - 1);
    mount->fs = fs;
    mount->active = 1;
    
    terminal_write("[VFS] Mounted at ");
    terminal_write(path);
    terminal_write("\n");
    
    return mount_count++;
}

int vfs_umount(int mount_id) {
    if (mount_id < 0 || mount_id >= mount_count) return -1;
    mounts[mount_id].active = 0;
    if (mounts[mount_id].fs && mounts[mount_id].fs->umount) {
        mounts[mount_id].fs->umount();
    }
    return 0;
}

int vfs_open(const char* path) {
    if (!path || fd_count >= VFS_MAX_FDS) return -1;
    
    vfs_mount_t* mount = find_mount_for_path(path);
    if (!mount || !mount->fs || !mount->fs->open) return -1;
    
    char rel_path[256];
    get_relative_path(path, mount, rel_path);
    
    int inode = mount->fs->open(rel_path);
    if (inode < 0) return -1;
    
    vfs_fd_t* fd = &fd_table[fd_count];
    fd->mount_id = mount - mounts;
    fd->inode = inode;
    fd->pos = 0;
    
    return fd_count++;
}

int vfs_create(const char* path, int type) {
    if (!path) return -1;
    
    vfs_mount_t* mount = find_mount_for_path(path);
    if (!mount || !mount->fs || !mount->fs->create) return -1;
    
    char rel_path[256];
    get_relative_path(path, mount, rel_path);
    
    return mount->fs->create(rel_path, type);
}

int vfs_read(int fd, void* buffer, uint32_t size) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !buffer) return -1;
    
    vfs_fd_t* file = &fd_table[fd];
    vfs_mount_t* mount = &mounts[file->mount_id];
    
    if (!mount->active || !mount->fs || !mount->fs->read) return -1;
    
    /* For now, simple read without seeking */
    return mount->fs->read(file->inode, buffer, size);
}

int vfs_write(int fd, const void* buffer, uint32_t size) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !buffer) return -1;
    
    vfs_fd_t* file = &fd_table[fd];
    vfs_mount_t* mount = &mounts[file->mount_id];
    
    if (!mount->active || !mount->fs || !mount->fs->write) return -1;
    
    return mount->fs->write(file->inode, buffer, size);
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FDS) return -1;
    memset(&fd_table[fd], 0, sizeof(vfs_fd_t));
    return 0;
}

int vfs_seek(int fd, uint32_t position) {
    if (fd < 0 || fd >= VFS_MAX_FDS) return -1;
    fd_table[fd].pos = position;
    return 0;
}

int vfs_delete(const char* path) {
    if (!path) return -1;
    
    /* Not implemented yet - for future use */
    return -1;
}

void vfs_list(const char* path, void (*callback)(const char*, uint32_t)) {
    if (!path || !callback) return;
    
    vfs_mount_t* mount = find_mount_for_path(path);
    if (!mount || !mount->fs || !mount->fs->list) return;
    
    char rel_path[256];
    get_relative_path(path, mount, rel_path);
    
    /* TODO: convert to inode and call list */
}

const char* vfs_getcwd(void) {
    return cwd;
}

int vfs_chdir(const char* path) {
    if (!path) return -1;
    
    /* Verify path exists by trying to open it */
    vfs_mount_t* mount = find_mount_for_path(path);
    if (!mount) return -1;
    
    strncpy(cwd, path, sizeof(cwd) - 1);
    return 0;
}
