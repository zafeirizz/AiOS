#include <stdint.h>
#include <string.h>
#include "../../include/fileviewer.h"
#include "../../include/wm.h"
#include "../../include/ui.h"
#include "../../include/gfx.h"
#include "../../include/heap.h"
#include "../../include/fat32.h"
#include "../../include/zfsx.h"
#include "../../include/myfs.h"

/* ── File Viewer State ───────────────────────────────────── */
typedef struct {
    char* content;
    ui_textarea_t textarea;
} fileviewer_state_t;

/* ── Draw callback ───────────────────────────────────────── */
static void fileviewer_draw(window_t* win, int cx, int cy, int cw, int ch) {
    (void)cw;  /* unused */
    fileviewer_state_t* state = (fileviewer_state_t*)win->app_data;
    if (!state) return;
    
    /* Clear client area */
    gfx_rect_fill(cx, cy, cw, ch, COLOR_WINBG); gfx_corner_brackets(cx+2,cy+2,cw-4,ch-4,8,COLOR_BORDER_DIM);
    
    /* Draw textarea with offset */
    ui_textarea_t offset_textarea = state->textarea;
    offset_textarea.x += cx;
    offset_textarea.y += cy;
    ui_textarea_draw(&offset_textarea);
}

/* ── Event callback ──────────────────────────────────────── */
static void fileviewer_event(window_t* win, const event_t* e) {
    fileviewer_state_t* state = (fileviewer_state_t*)win->app_data;
    if (!state) return;
    
    int cx, cy, cw, ch;
    wm_client_rect(win, &cx, &cy, &cw, &ch);
    
    /* Forward events to textarea */
    ui_textarea_event(&state->textarea, e);
}

/* ── App launcher ───────────────────────────────────────── */
int fileviewer_launch(const char* filename) {
    if (!filename) return -1;
    
    /* Try to read from FAT32 */
    uint8_t* buf = (uint8_t*)kmalloc(4096);  /* 4KB buffer */
    if (!buf) return -1;
    
    int bytes = -1;
    /* Try filesystems in order */
    if (zfsx_is_mounted()) {
        bytes = zfsx_read_file(filename, buf, 4096);
    }
    if (bytes < 0 && fat32_is_mounted()) {
        bytes = fat32_read_file(filename, buf, 4096);
    }
    if (bytes < 0) {
        bytes = myfs_read(filename, buf, 4096);
    }
    
    if (bytes < 0) {
        kfree(buf);
        return -1;  /* File not found */
    }
    
    /* Null-terminate */
    buf[bytes] = 0;
    
    /* Create state */
    fileviewer_state_t* state = (fileviewer_state_t*)kmalloc(sizeof(fileviewer_state_t));
    if (!state) {
        kfree(buf);
        return -1;
    }
    
    memset(state, 0, sizeof(fileviewer_state_t));
    state->content = (char*)buf;
    
    /* Create textarea */
    state->textarea = ui_textarea_create(10, 10, 380, 280, (char*)buf, 4096);
    
    /* Create window */
    char title[64];
    strcpy(title, "File Viewer");
    window_t* win = wm_create(title, 100, 100, 400, 320, fileviewer_draw, fileviewer_event);
    if (!win) {
        kfree(state->content);
        kfree(state);
        return -1;
    }
    
    win->app_data = state;
    return 0;
}