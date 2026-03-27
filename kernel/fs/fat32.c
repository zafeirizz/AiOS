#include <stdint.h>
#include "../../include/fat32.h"
#include "../../include/ata.h"
#include "../../include/string.h"
#include "../../include/vga.h"

/* ── On-disk structures ───────────────────────────────── */

typedef struct {
    uint8_t  jump[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} __attribute__((packed)) bpb_t;

typedef struct {
    uint8_t  status;
    uint8_t  chs_first[3];
    uint8_t  type;
    uint8_t  chs_last[3];
    uint32_t lba_start;
    uint32_t lba_size;
} __attribute__((packed)) mbr_part_t;

typedef struct {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  ctime_tenth;
    uint16_t ctime;
    uint16_t cdate;
    uint16_t adate;
    uint16_t cluster_hi;
    uint16_t mtime;
    uint16_t mdate;
    uint16_t cluster_lo;
    uint32_t file_size;
} __attribute__((packed)) dirent_t;

#define ATTR_READONLY   0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LFN        0x0F   /* all four low bits set */

#define FAT_EOC         0x0FFFFFF8   /* end-of-chain marker */

/* ── Mount state ──────────────────────────────────────── */

static uint32_t vol_lba;          /* LBA of the BPB sector        */
static uint32_t bps;              /* bytes per sector (usually 512)*/
static uint32_t spc;              /* sectors per cluster           */
static uint32_t fat_lba;          /* LBA of FAT region             */
static uint32_t data_lba;         /* LBA of data region            */
static uint32_t root_cluster;
static uint32_t total_clusters;
static uint8_t  vol_label[12];
static int      mounted = 0;

static uint8_t  sec[ATA_SECTOR_SIZE];   /* scratch sector buffer */

/* ── Low-level helpers ────────────────────────────────── */

static uint32_t clus2lba(uint32_t c) {
    return data_lba + (c - 2) * spc;
}

static uint32_t fat_entry(uint32_t cluster) {
    uint32_t offset = cluster * 4;
    uint32_t lba    = fat_lba + offset / bps;
    uint32_t off    = offset % bps;
    if (ata_read(lba, 1, sec) < 0) return 0x0FFFFFFF;
    uint32_t val;
    memcpy(&val, sec + off, 4);
    return val & 0x0FFFFFFF;
}

/* ── 8.3 name helpers ─────────────────────────────────── */

/* Convert raw 8.3 dir entry name to "NAME.EXT" string in out[13] */
static void name83_to_str(const uint8_t* raw, char* out) {
    int i = 0, n = 0;
    for (int j = 0; j < 8 && raw[j] != ' '; j++)
        out[n++] = (char)raw[j];
    if (raw[8] != ' ') {
        out[n++] = '.';
        for (int j = 8; j < 11 && raw[j] != ' '; j++)
            out[n++] = (char)raw[j];
    }
    out[n] = '\0';
    /* Lowercase the whole thing for readability */
    for (i = 0; out[i]; i++)
        if (out[i] >= 'A' && out[i] <= 'Z') out[i] += 32;
}

/*
 * Convert a user-supplied path component (e.g. "README.TXT")
 * to the padded uppercase 11-byte 8.3 form used in directory entries.
 */
static void str_to_name83(const char* src, uint8_t* out) {
    memset(out, ' ', 11);
    int si = 0, di = 0;
    /* Name part (up to 8 chars) */
    while (src[si] && src[si] != '.' && di < 8) {
        char c = src[si++];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[di++] = (uint8_t)c;
    }
    /* Skip the dot if present */
    if (src[si] == '.') {
        si++;
        di = 8;
        while (src[si] && di < 11) {
            char c = src[si++];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[di++] = (uint8_t)c;
        }
    }
}

/* ── Mount ────────────────────────────────────────────── */

int fat32_mount(uint32_t lba_start) {
    if (ata_read(lba_start, 1, sec) < 0) return -1;

    /* Boot sector signature */
    if (sec[510] != 0x55 || sec[511] != 0xAA) return -1;

    bpb_t bpb;
    memcpy(&bpb, sec, sizeof(bpb));

    /* Must be FAT32: fat_size_16==0, root_entry_count==0 */
    if (bpb.fat_size_16 != 0 || bpb.root_entry_count != 0) return -1;
    if (bpb.fat_size_32 == 0) return -1;

    /* Sanity: bytes-per-sector must be 512 for our driver */
    if (bpb.bytes_per_sector != 512) return -1;

    vol_lba      = lba_start;
    bps          = bpb.bytes_per_sector;
    spc          = bpb.sectors_per_cluster;
    fat_lba      = lba_start + bpb.reserved_sectors;
    data_lba     = fat_lba + bpb.num_fats * bpb.fat_size_32;
    root_cluster = bpb.root_cluster;

    uint32_t data_sectors = (bpb.total_sectors_32
                             ? bpb.total_sectors_32
                             : bpb.total_sectors_16)
                            - (data_lba - lba_start);
    total_clusters = data_sectors / spc;

    /* Volume label */
    memcpy(vol_label, bpb.volume_label, 11);
    vol_label[11] = '\0';

    mounted = 1;
    return 0;
}

int fat32_mount_mbr(void) {
    /* Read MBR (LBA 0) */
    if (ata_read(0, 1, sec) < 0) return -1;
    if (sec[510] != 0x55 || sec[511] != 0xAA) return -1;

    /* Partition table starts at offset 446 — four 16-byte entries */
    mbr_part_t parts[4];
    memcpy(parts, sec + 446, sizeof(parts));

    for (int i = 0; i < 4; i++) {
        /* FAT32 with CHS (0x0B) or FAT32 with LBA (0x0C) */
        if (parts[i].type == 0x0B || parts[i].type == 0x0C) {
            if (parts[i].lba_start == 0) continue;
            terminal_write("MBR: found FAT32 partition at LBA ");
            terminal_write_dec(parts[i].lba_start);
            terminal_write(", size ");
            terminal_write_dec(parts[i].lba_size);
            terminal_writeline(" sectors");
            return fat32_mount(parts[i].lba_start);
        }
    }
    terminal_writeline("MBR: no FAT32 partition found (types 0x0B/0x0C)");
    return -1;
}

/* ── Info ─────────────────────────────────────────────── */

void fat32_info(void) {
    if (!mounted) { terminal_writeline("FAT32: not mounted"); return; }
    terminal_write("Volume label : "); terminal_writeline((char*)vol_label);
    terminal_write("Volume LBA   : "); terminal_write_dec(vol_lba);   terminal_putchar('\n');
    terminal_write("Cluster size : "); terminal_write_dec(spc * bps); terminal_writeline(" bytes");
    terminal_write("Total clusters: "); terminal_write_dec(total_clusters); terminal_putchar('\n');
}

/* ── Directory iteration ──────────────────────────────── */

/*
 * Walk a cluster chain for a directory, calling cb() for each
 * non-LFN, non-volume-id entry. Stops at end-of-directory.
 */
static void walk_dir(uint32_t start_cluster,
                     void (*cb)(const char* name, uint32_t size))
{
    uint32_t cluster = start_cluster;

    while (cluster < FAT_EOC) {
        uint32_t lba = clus2lba(cluster);
        for (uint32_t s = 0; s < spc; s++) {
            if (ata_read(lba + s, 1, sec) < 0) return;
            dirent_t* entries = (dirent_t*)sec;
            int per_sec = (int)(bps / sizeof(dirent_t));

            for (int i = 0; i < per_sec; i++) {
                dirent_t* e = &entries[i];
                if (e->name[0] == 0x00) return;         /* end of dir */
                if (e->name[0] == 0xE5) continue;       /* deleted    */
                if (e->attr == ATTR_LFN) continue;      /* LFN entry  */
                if (e->attr & ATTR_VOLUME_ID) continue; /* volume id  */

                char name[13];
                name83_to_str(e->name, name);
                cb(name, e->file_size);
            }
        }
        cluster = fat_entry(cluster);
    }
}

void fat32_list(void (*cb)(const char* name, uint32_t size)) {
    if (!mounted) { terminal_writeline("FAT32: not mounted"); return; }
    walk_dir(root_cluster, cb);
}

/* ── File read ────────────────────────────────────────── */

int fat32_read_file(const char* path, uint8_t* buf, uint32_t buf_size) {
    if (!mounted) return -1;

    /* Strip leading slash */
    if (path[0] == '/') path++;

    /* Convert name to 8.3 */
    uint8_t name83[11];
    str_to_name83(path, name83);

    /* Search root directory for the entry */
    dirent_t target = {0};
    int found = 0;
    uint32_t cluster = root_cluster;

    while (!found && cluster < FAT_EOC) {
        uint32_t lba = clus2lba(cluster);
        for (uint32_t s = 0; s < spc && !found; s++) {
            if (ata_read(lba + s, 1, sec) < 0) return -1;
            dirent_t* entries = (dirent_t*)sec;
            int per_sec = (int)(bps / sizeof(dirent_t));

            for (int i = 0; i < per_sec; i++) {
                dirent_t* e = &entries[i];
                if (e->name[0] == 0x00) goto done_search;
                if (e->name[0] == 0xE5) continue;
                if (e->attr == ATTR_LFN) continue;
                if (e->attr & (ATTR_DIRECTORY | ATTR_VOLUME_ID)) continue;

                if (memcmp(e->name, name83, 11) == 0) {
                    target = *e;
                    found  = 1;
                    break;
                }
            }
        }
        cluster = fat_entry(cluster);
    }
done_search:
    if (!found) return -1;

    /* Read data clusters */
    uint32_t remaining = target.file_size;
    uint32_t written   = 0;
    cluster = ((uint32_t)target.cluster_hi << 16) | target.cluster_lo;

    while (cluster < FAT_EOC && remaining > 0) {
        uint32_t lba = clus2lba(cluster);
        for (uint32_t s = 0; s < spc && remaining > 0; s++) {
            if (ata_read(lba + s, 1, sec) < 0) return (int)written;
            uint32_t chunk = remaining < bps ? remaining : bps;
            if (written + chunk > buf_size) chunk = buf_size - written;
            memcpy(buf + written, sec, chunk);
            written   += chunk;
            remaining -= chunk;
        }
        cluster = fat_entry(cluster);
    }
    return (int)written;
}

int fat32_delete(const char* path) {
    (void)path;
    return -1; /* Not implemented yet */
}

int fat32_is_mounted(void) {
    return mounted;
}
