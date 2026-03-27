#include <stdint.h>
#include <string.h>
#include "../../include/diskmgr.h"
#include "../../include/wm.h"
#include "../../include/ui.h"
#include "../../include/gfx.h"
#include "../../include/heap.h"
#include "../../include/ata.h"
#include "../../include/myfs.h"
#include "../../include/fat32.h"
#include "../../include/zfsx.h"
#include "../../include/vfs.h"

typedef struct {
    ui_button_t btn_refresh;
    ui_button_t btn_mkfs_myfs;
    ui_button_t btn_mkfs_fat;
    ui_button_t btn_mkfs_zfsx;
    ui_button_t btn_mount_myfs;
    ui_button_t btn_mount_fat;
    ui_button_t btn_mount_zfsx;
    char        status[128];
    int         myfs_ok;
    int         fat32_ok;
    int         zfsx_ok;
} diskmgr_state_t;

/* ── Draw a horizontal bar (0-100 percent) ────────────── */
static void draw_bar(int x, int y, int w, int h, int pct, uint32_t color) {
    gfx_rect(x, y, w, h, COLOR_BORDER);
    int fill = (w - 2) * pct / 100;
    if (fill > 0) gfx_rect_fill(x+1, y+1, fill, h-2, color);
}

/* ── Draw callback ────────────────────────────────────── */
static void diskmgr_draw(window_t* win, int cx, int cy, int cw, int ch) {
    diskmgr_state_t* state = (diskmgr_state_t*)win->app_data;
    if (!state) return;

    gfx_rect_fill(cx, cy, cw, ch, COLOR_WINBG);

    /* Title */
    gfx_text(cx+10, cy+8, "Disk Manager", COLOR_TEXT_L);
    gfx_hline(cx, cy+24, cw, COLOR_BORDER);

    int y = cy + 30;

    /* ATA disk info */
    gfx_text(cx+10, y, "ATA Primary  IDE  512 MB", COLOR_ACCENT);
    y += 18;

    /* ── MyFS row ── */
    gfx_text(cx+10, y, "MyFS  [LBA 0]", COLOR_TEXT_L);
    const char* myfs_st = state->myfs_ok ? "mounted" : "unmounted";
    uint32_t myfs_col   = state->myfs_ok ? COLOR_GREEN : COLOR_GRAY;
    gfx_text(cx+130, y, myfs_st, myfs_col);
    draw_bar(cx+220, y, 120, 10, state->myfs_ok ? 35 : 0, COLOR_ACCENT);
    gfx_text(cx+346, y, "35%", COLOR_GRAY);
    y += 16;

    /* ── FAT32 row ── */
    gfx_text(cx+10, y, "FAT32 [LBA 4096]", COLOR_TEXT_L);
    const char* fat_st = state->fat32_ok ? "mounted" : "unmounted";
    uint32_t fat_col   = state->fat32_ok ? COLOR_GREEN : COLOR_GRAY;
    gfx_text(cx+130, y, fat_st, fat_col);
    draw_bar(cx+220, y, 120, 10, state->fat32_ok ? 12 : 0, COLOR_YELLOW);
    gfx_text(cx+346, y, "12%", COLOR_GRAY);
    y += 16;

    /* ── ZFSX row ── */
    gfx_text(cx+10, y, "ZFSX  [LBA 8192]", COLOR_TEXT_L);
    const char* zfsx_st = state->zfsx_ok ? "mounted" : "unmounted";
    uint32_t zfsx_col   = state->zfsx_ok ? COLOR_GREEN : COLOR_GRAY;
    gfx_text(cx+130, y, zfsx_st, zfsx_col);
    draw_bar(cx+220, y, 120, 10, state->zfsx_ok ? 5 : 0, COLOR_CYAN);
    gfx_text(cx+346, y, "5%",  COLOR_GRAY);
    y += 22;

    gfx_hline(cx, y, cw, COLOR_BORDER);
    y += 6;

    /* ── Total disk bar ── */
    gfx_text(cx+10, y, "Disk usage:", COLOR_TEXT_L);
    y += 14;
    /* Composite bar: each FS gets proportional width */
    int bx = cx+10, bw = cw-20, bh = 14;
    gfx_rect(bx, y, bw, bh, COLOR_BORDER);
    int myfs_w  = bw * 35 / 100;
    int fat_w   = bw * 12 / 100;
    int zfsx_w  = bw * 5  / 100;
    if (state->myfs_ok)  gfx_rect_fill(bx+1,          y+1, myfs_w,  bh-2, COLOR_ACCENT);
    if (state->fat32_ok) gfx_rect_fill(bx+1+myfs_w,   y+1, fat_w,   bh-2, COLOR_YELLOW);
    if (state->zfsx_ok)  gfx_rect_fill(bx+1+myfs_w+fat_w, y+1, zfsx_w, bh-2, COLOR_CYAN);
    y += bh + 8;

    /* Legend */
    gfx_rect_fill(cx+10, y+2, 10, 8, COLOR_ACCENT);  gfx_text(cx+24, y, "MyFS",  COLOR_TEXT_L);
    gfx_rect_fill(cx+80, y+2, 10, 8, COLOR_YELLOW);  gfx_text(cx+94, y, "FAT32", COLOR_TEXT_L);
    gfx_rect_fill(cx+155,y+2, 10, 8, COLOR_CYAN);    gfx_text(cx+169,y, "ZFSX",  COLOR_TEXT_L);
    gfx_rect_fill(cx+225,y+2, 10, 8, COLOR_LGRAY);   gfx_text(cx+239,y, "Free",  COLOR_TEXT_L);
    y += 18;

    gfx_hline(cx, y, cw, COLOR_BORDER);
    y += 6;

    /* ── Operations ── */
    gfx_text(cx+10, y, "Format:", COLOR_TEXT_L);
    ui_button_t b;
    b = state->btn_mkfs_myfs;  b.x+=cx; b.y+=cy; ui_button_draw(&b);
    b = state->btn_mkfs_fat;   b.x+=cx; b.y+=cy; ui_button_draw(&b);
    b = state->btn_mkfs_zfsx;  b.x+=cx; b.y+=cy; ui_button_draw(&b);
    y += 34;

    gfx_text(cx+10, y, "Mount:", COLOR_TEXT_L);
    b = state->btn_mount_myfs; b.x+=cx; b.y+=cy; ui_button_draw(&b);
    b = state->btn_mount_fat;  b.x+=cx; b.y+=cy; ui_button_draw(&b);
    b = state->btn_mount_zfsx; b.x+=cx; b.y+=cy; ui_button_draw(&b);
    y += 34;

    b = state->btn_refresh;    b.x+=cx; b.y+=cy; ui_button_draw(&b);

    /* Status */
    gfx_rect_fill(cx, cy+ch-18, cw, 18, COLOR_TITLEBAR);
    gfx_text(cx+6, cy+ch-14, state->status, COLOR_TEXT_D);
}

/* ── Event callback ───────────────────────────────────── */
static void diskmgr_event(window_t* win, const event_t* e) {
    diskmgr_state_t* state = (diskmgr_state_t*)win->app_data;
    if (!state) return;
    int cx, cy, cw, ch;
    wm_client_rect(win, &cx, &cy, &cw, &ch);

    ui_button_event(&state->btn_refresh,    e, 0, 0);
    ui_button_event(&state->btn_mkfs_myfs,  e, 0, 0);
    ui_button_event(&state->btn_mkfs_fat,   e, 0, 0);
    ui_button_event(&state->btn_mkfs_zfsx,  e, 0, 0);
    ui_button_event(&state->btn_mount_myfs, e, 0, 0);
    ui_button_event(&state->btn_mount_fat,  e, 0, 0);
    ui_button_event(&state->btn_mount_zfsx, e, 0, 0);

    if (state->btn_refresh.clicked) {
        state->btn_refresh.clicked = 0;
        state->myfs_ok  = myfs_is_mounted();
        state->fat32_ok = fat32_is_mounted();
        state->zfsx_ok  = zfsx_is_mounted();
        strcpy(state->status, "Refreshed.");
        wm_invalidate(win);
    }

    if (state->btn_mkfs_myfs.clicked) {
        state->btn_mkfs_myfs.clicked = 0;
        int r = myfs_format(0, 4096);
        strcpy(state->status, r==0 ? "MyFS formatted OK." : "MyFS format failed.");
        wm_invalidate(win);
    }
    if (state->btn_mkfs_fat.clicked) {
        state->btn_mkfs_fat.clicked = 0;
        /* FAT32 format: tell user to recreate disk.img */
        strcpy(state->status, "FAT32: use 'mkfs.fat' on disk.img.");
        wm_invalidate(win);
    }
    if (state->btn_mkfs_zfsx.clicked) {
        state->btn_mkfs_zfsx.clicked = 0;
        int r = zfsx_format(8192, 20000);
        strcpy(state->status, r==0 ? "ZFSX formatted OK." : "ZFSX format failed.");
        wm_invalidate(win);
    }

    if (state->btn_mount_myfs.clicked) {
        state->btn_mount_myfs.clicked = 0;
        int r = myfs_mount(0);
        state->myfs_ok = (r == 0);
        strcpy(state->status, r==0 ? "MyFS mounted." : "MyFS mount failed.");
        wm_invalidate(win);
    }
    if (state->btn_mount_fat.clicked) {
        state->btn_mount_fat.clicked = 0;
        int r = fat32_mount(4096);
        state->fat32_ok = (r == 0);
        strcpy(state->status, r==0 ? "FAT32 mounted." : "FAT32 mount failed.");
        wm_invalidate(win);
    }
    if (state->btn_mount_zfsx.clicked) {
        state->btn_mount_zfsx.clicked = 0;
        int r = zfsx_mount(8192);
        state->zfsx_ok = (r == 0);
        strcpy(state->status, r==0 ? "ZFSX mounted." : "ZFSX mount failed.");
        wm_invalidate(win);
    }
}

/* ── Launch ───────────────────────────────────────────── */
int diskmgr_launch(void) {
    diskmgr_state_t* state =
        (diskmgr_state_t*)kmalloc(sizeof(diskmgr_state_t));
    if (!state) return -1;
    memset(state, 0, sizeof(diskmgr_state_t));

    state->myfs_ok  = myfs_is_mounted();
    state->fat32_ok = fat32_is_mounted();
    state->zfsx_ok  = zfsx_is_mounted();
    strcpy(state->status, "Ready.");

    int row1_y = 172, row2_y = 206;
    state->btn_mkfs_myfs  = ui_button_create(60,  row1_y, 72, 22, "MyFS");
    state->btn_mkfs_fat   = ui_button_create(136, row1_y, 72, 22, "FAT32");
    state->btn_mkfs_zfsx  = ui_button_create(212, row1_y, 72, 22, "ZFSX");
    state->btn_mount_myfs = ui_button_create(60,  row2_y, 72, 22, "MyFS");
    state->btn_mount_fat  = ui_button_create(136, row2_y, 72, 22, "FAT32");
    state->btn_mount_zfsx = ui_button_create(212, row2_y, 72, 22, "ZFSX");
    state->btn_refresh    = ui_button_create(290, row2_y, 72, 22, "Refresh");

    window_t* win = wm_create("Disk Manager", 550, 60, 390, 270,
                              diskmgr_draw, diskmgr_event);
    if (!win) { kfree(state); return -1; }
    win->app_data = state;
    return 0;
}
