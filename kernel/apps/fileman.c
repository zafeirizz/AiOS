#include <stdint.h>
#include <string.h>
#include "../../include/fileman.h"
#include "../../include/wm.h"
#include "../../include/ui.h"
#include "../../include/gfx.h"
#include "../../include/myfs.h"
#include "../../include/heap.h"
#include "../../include/fileviewer.h"
#include "../../include/fat32.h"
#include "../../include/zfsx.h"
#include "../../include/vfs.h"
#include "../../include/string.h"

/* ── File Manager State ─────────────────────────────────── */
typedef struct {
    const char** file_names;
    char         sizes[256][16];
    char         fs_tags[256][8];
    int          file_count;
    int          is_dir[256];
    ui_list_t    list;
    ui_button_t  btn_open;
    ui_button_t  btn_delete;
    ui_button_t  btn_new;
    ui_button_t  btn_refresh;
    ui_button_t  btn_rename;
    char         cwd[64];            /* current FS view: "myfs","fat32","zfsx" */
    char         new_name_buf[64];
    int          rename_mode;
    int          selected_idx;
} fileman_state_t;

static const char*  g_temp_files[256];
static char         g_temp_names[256][64];
static char         g_temp_sizes[256][16];
static char         g_temp_fstag[256][8];
static int          g_temp_isdir[256];
static int          g_temp_count = 0;

static void fm_cb_myfs(const char* name, uint32_t size) {
    if (g_temp_count >= 256) return;
    strncpy(g_temp_names[g_temp_count], name, 63);
    g_temp_names[g_temp_count][63] = '\0';
    g_temp_files[g_temp_count] = g_temp_names[g_temp_count];
    /* format size */
    if (size >= 1024) {
        char tmp[8]; int kb = (int)(size/1024);
        int i=0; int v=kb;
        if(!v){tmp[i++]='0';}else{char r[8];int ri=0;while(v>0){r[ri++]='0'+(v%10);v/=10;}while(ri-->0)tmp[i++]=r[ri];}
        tmp[i++]='K'; tmp[i]='\0';
        strncpy(g_temp_sizes[g_temp_count], tmp, 15);
    } else {
        char tmp[8]; int i=0; int v=(int)size;
        if(!v){tmp[i++]='0';}else{char r[8];int ri=0;while(v>0){r[ri++]='0'+(v%10);v/=10;}while(ri-->0)tmp[i++]=r[ri];}
        tmp[i++]='B'; tmp[i]='\0';
        strncpy(g_temp_sizes[g_temp_count], tmp, 15);
    }
    strcpy(g_temp_fstag[g_temp_count], "MyFS");
    g_temp_isdir[g_temp_count] = 0;
    g_temp_count++;
}

static void fm_cb_fat32(const char* name, uint32_t size) {
    if (g_temp_count >= 256) return;
    strncpy(g_temp_names[g_temp_count], name, 63);
    g_temp_names[g_temp_count][63] = '\0';
    g_temp_files[g_temp_count] = g_temp_names[g_temp_count];
    strncpy(g_temp_sizes[g_temp_count], "...", 15);
    (void)size;
    strcpy(g_temp_fstag[g_temp_count], "FAT32");
    g_temp_isdir[g_temp_count] = 0;
    g_temp_count++;
}

static void fm_cb_zfsx(const char* name, uint32_t size) {
    if (g_temp_count >= 256) return;
    strncpy(g_temp_names[g_temp_count], name, 63);
    g_temp_names[g_temp_count][63] = '\0';
    g_temp_files[g_temp_count] = g_temp_names[g_temp_count];
    strncpy(g_temp_sizes[g_temp_count], "...", 15);
    (void)size;
    strcpy(g_temp_fstag[g_temp_count], "ZFSX");
    g_temp_isdir[g_temp_count] = 0;
    g_temp_count++;
}

static void fileman_refresh(fileman_state_t* state) {
    g_temp_count = 0;

    if (strcmp(state->cwd, "fat32") == 0) {
        if (fat32_is_mounted()) fat32_list(fm_cb_fat32);
    } else if (strcmp(state->cwd, "zfsx") == 0) {
        if (zfsx_is_mounted()) zfsx_list_root(fm_cb_zfsx);
    } else {
        /* Default: show all — MyFS first, then FAT32, then ZFSX */
        myfs_list(fm_cb_myfs);
        if (fat32_is_mounted()) fat32_list(fm_cb_fat32);
        if (zfsx_is_mounted()) zfsx_list_root(fm_cb_zfsx);
    }

    /* Copy pointers and metadata into state */
    for (int i = 0; i < g_temp_count; i++) {
        state->sizes[i][0] = '\0';
        strncpy(state->sizes[i], g_temp_sizes[i], 15);
        strncpy(state->fs_tags[i], g_temp_fstag[i], 7);
        state->is_dir[i] = g_temp_isdir[i];
    }

    state->file_names  = (const char**)g_temp_files;
    state->file_count  = g_temp_count;
    state->selected_idx = -1;
    ui_list_set_items(&state->list, state->file_names, state->file_count);
    state->list.selected = -1;
}

/* ── Draw ─────────────────────────────────────────────────── */
static void fileman_draw(window_t* win, int cx, int cy, int cw, int ch) {
    fileman_state_t* state = (fileman_state_t*)win->app_data;
    if (!state) return;

    gfx_rect_fill(cx, cy, cw, ch, COLOR_WINBG);

    /* FS selector tabs */
    const char* tabs[] = { "All", "MyFS", "FAT32", "ZFSX" };
    const char* cwds[] = { "all", "myfs", "fat32", "zfsx" };
    int tx = cx + 4;
    for (int i = 0; i < 4; i++) {
        int is_active = (strcmp(state->cwd, cwds[i]) == 0) ||
                        (i==0 && strcmp(state->cwd,"all")==0) ||
                        (i==0 && state->cwd[0]=='\0');
        uint32_t tbg = is_active ? COLOR_ACCENT : COLOR_BORDER;
        uint32_t tfg = is_active ? COLOR_TEXT_D : COLOR_TEXT_L;
        int tw = gfx_text_width(tabs[i]) + 12;
        gfx_rect_fill(tx, cy + 2, tw, 18, tbg);
        gfx_rect(tx, cy + 2, tw, 18, COLOR_BORDER);
        gfx_text(tx + 6, cy + 5, tabs[i], tfg);
        tx += tw + 2;
    }

    /* Separator under tabs */
    gfx_hline(cx, cy + 22, cw, COLOR_BORDER);

    /* Path label */
    char path_label[80];
    strcpy(path_label, "Location: /");
    strcat(path_label, state->cwd[0] ? state->cwd : "");
    gfx_text(cx + 4, cy + 26, path_label, COLOR_TEXT_L);

    /* File list */
    ui_list_t offset_list = state->list;
    offset_list.x += cx;
    offset_list.y += cy + 44;
    offset_list.h   = ch - 44 - 36;
    ui_list_draw(&offset_list);

    /* Size / FS annotations beside selected item */
    if (state->list.selected >= 0 && state->list.selected < state->file_count) {
        int idx = state->list.selected;
        int iy  = cy + 44 + (idx - state->list.scroll_y) * state->list.item_h;
        if (iy >= cy + 44 && iy < cy + 44 + offset_list.h) {
            int tag_x = cx + offset_list.w - 80;
            gfx_text(tag_x, iy + 2, state->sizes[idx],   COLOR_GRAY);
            gfx_text(tag_x + 36, iy + 2, state->fs_tags[idx], COLOR_ACCENT);
        }
    }

    /* Button row at bottom */
    ui_button_t btn;
    btn = state->btn_open;    btn.x += cx; btn.y += cy; ui_button_draw(&btn);
    btn = state->btn_delete;  btn.x += cx; btn.y += cy; ui_button_draw(&btn);
    btn = state->btn_new;     btn.x += cx; btn.y += cy; ui_button_draw(&btn);
    btn = state->btn_refresh; btn.x += cx; btn.y += cy; ui_button_draw(&btn);
    btn = state->btn_rename;  btn.x += cx; btn.y += cy; ui_button_draw(&btn);

    /* Rename input overlay */
    if (state->rename_mode) {
        gfx_rect_fill(cx + 4, cy + ch - 56, 240, 20, COLOR_WINBG);
        gfx_rect(cx + 4, cy + ch - 56, 240, 20, COLOR_ACCENT);
        gfx_text(cx + 8, cy + ch - 52, state->new_name_buf, COLOR_TEXT_L);
        gfx_text(cx + 8 + gfx_text_width(state->new_name_buf),
                 cy + ch - 52, "_", COLOR_ACCENT);
        gfx_text(cx + 250, cy + ch - 52, "[Enter=OK  Esc=Cancel]", COLOR_GRAY);
    }

    /* Status bar */
    char status[64];
    char num_str[8];
    int v = state->file_count; int si = 0;
    char r[8]; int ri = 0;
    if (!v) { r[ri++]='0'; }
    else { 
        int vv=v; 
        while(vv>0){r[ri++]='0'+(vv%10);vv/=10;} 
    }
    while(ri > 0) {
        num_str[si++] = r[--ri];
    }
    num_str[si] = '\0';
    strcpy(status, num_str);
    strcat(status, " items");
    if (state->list.selected >= 0) {
        strcat(status, "  Selected: ");
        strcat(status, state->file_names[state->list.selected]);
    }
    gfx_rect_fill(cx, cy + ch - 16, cw, 16, COLOR_TITLEBAR);
    gfx_text(cx + 4, cy + ch - 14, status, COLOR_TEXT_D);
}

/* ── Event ───────────────────────────────────────────────── */
static void fileman_event(window_t* win, const event_t* e) {
    fileman_state_t* state = (fileman_state_t*)win->app_data;
    if (!state) return;

    /* Rename keyboard input */
    if (state->rename_mode) {
        if (e->type == EVT_KEY_DOWN) {
            char c = e->key.c;
            if (c == '\n') {
                /* Commit rename: only MyFS supported currently */
                if (state->list.selected >= 0 && strlen(state->new_name_buf) > 0) {
                    const char* old_name = state->file_names[state->list.selected];
                    uint8_t buf[4096];
                    int bytes = myfs_read(old_name, buf, sizeof(buf));
                    if (bytes > 0) {
                        myfs_write(state->new_name_buf, buf, (uint32_t)bytes);
                        myfs_delete(old_name);
                    }
                }
                state->rename_mode = 0;
                fileman_refresh(state);
                wm_invalidate(win);
            } else if (c == 27) {
                state->rename_mode = 0;
                wm_invalidate(win);
            } else if (c == '\b') {
                int l = (int)strlen(state->new_name_buf);
                if (l > 0) state->new_name_buf[l-1] = '\0';
                wm_invalidate(win);
            } else if (c >= 32 && c < 127) {
                int l = (int)strlen(state->new_name_buf);
                if (l < 63) { state->new_name_buf[l] = c; state->new_name_buf[l+1] = '\0'; }
                wm_invalidate(win);
            }
        }
        return;
    }

    int cx, cy, cw, ch;
    wm_client_rect(win, &cx, &cy, &cw, &ch);

    /* Tab clicks */
    if (e->type == EVT_MOUSE_DOWN && e->mouse.button == 0) {
        int my = e->mouse.y + cy;
        int mx = e->mouse.x + cx;
        if (my >= cy + 2 && my <= cy + 20) {
            const char* tabs[] = { "All", "MyFS", "FAT32", "ZFSX" };
            const char* cwds[] = { "all", "myfs", "fat32", "zfsx" };
            int tx = cx + 4;
            for (int i = 0; i < 4; i++) {
                int tw = gfx_text_width(tabs[i]) + 12;
                if (mx >= tx && mx < tx + tw) {
                    strcpy(state->cwd, cwds[i]);
                    fileman_refresh(state);
                    wm_invalidate(win);
                    break;
                }
                tx += tw + 2;
            }
        }
    }

    /* Forward to list (offset by tab height) */
    ui_list_event(&state->list, e, 0, 0);
    if (state->list.clicked) {
        state->list.clicked = 0;
        state->selected_idx = state->list.clicked_idx;
        wm_invalidate(win);
    }

    /* Buttons */
    ui_button_event(&state->btn_open,    e, 0, 0);
    ui_button_event(&state->btn_delete,  e, 0, 0);
    ui_button_event(&state->btn_new,     e, 0, 0);
    ui_button_event(&state->btn_refresh, e, 0, 0);
    ui_button_event(&state->btn_rename,  e, 0, 0);

    if (state->btn_open.clicked) {
        state->btn_open.clicked = 0;
        if (state->list.selected >= 0) {
            const char* name = state->file_names[state->list.selected];
            fileviewer_launch(name);
        }
    }

    if (state->btn_delete.clicked) {
        state->btn_delete.clicked = 0;
        if (state->list.selected >= 0) {
            const char* name = state->file_names[state->list.selected];
            const char* fs   = state->fs_tags[state->list.selected];
            if (strcmp(fs, "FAT32") == 0) {
                fat32_delete(name);
            } else if (strcmp(fs, "ZFSX") == 0) {
                int id = zfsx_find_in_dir(ZFSX_ROOT_ID, name);
                if (id >= 0) zfsx_delete(id);
            } else {
                myfs_delete(name);
            }
            fileman_refresh(state);
            wm_invalidate(win);
        }
    }

    if (state->btn_new.clicked) {
        state->btn_new.clicked = 0;
        /* Create "newfile.txt" in default (MyFS) */
        const char* defname = "newfile.txt";
        myfs_write(defname, (const uint8_t*)"", 0);
        fileman_refresh(state);
        wm_invalidate(win);
    }

    if (state->btn_rename.clicked) {
        state->btn_rename.clicked = 0;
        if (state->list.selected >= 0) {
            strncpy(state->new_name_buf,
                    state->file_names[state->list.selected], 63);
            state->rename_mode = 1;
            wm_invalidate(win);
        }
    }

    if (state->btn_refresh.clicked) {
        state->btn_refresh.clicked = 0;
        fileman_refresh(state);
        wm_invalidate(win);
    }
}

/* ── Launch ──────────────────────────────────────────────── */
int fileman_launch(void) {
    fileman_state_t* state =
        (fileman_state_t*)kmalloc(sizeof(fileman_state_t));
    if (!state) return -1;
    memset(state, 0, sizeof(fileman_state_t));
    strcpy(state->cwd, "all");

    int btn_y = 280;
    state->list       = ui_list_create(4, 44, 392, 232, 18);
    state->btn_open   = ui_button_create(4,   btn_y, 70, 24, "Open");
    state->btn_delete = ui_button_create(80,  btn_y, 70, 24, "Delete");
    state->btn_new    = ui_button_create(156, btn_y, 70, 24, "New File");
    state->btn_rename = ui_button_create(232, btn_y, 70, 24, "Rename");
    state->btn_refresh= ui_button_create(308, btn_y, 80, 24, "Refresh");

    fileman_refresh(state);

    window_t* win = wm_create("File Manager", 60, 60, 400, 330,
                              fileman_draw, fileman_event);
    if (win) win->app_data = state;
    return 0;
}
