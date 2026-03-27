#include <stdint.h>
#include <string.h>
#include "../../include/notepad.h"
#include "../../include/wm.h"
#include "../../include/ui.h"
#include "../../include/gfx.h"
#include "../../include/myfs.h"
#include "../../include/fat32.h"
#include "../../include/zfsx.h"
#include "../../include/heap.h"

/* ── Notepad State ───────────────────────────────────── */
typedef struct {
    ui_textarea_t textarea;
    ui_button_t btn_save;
    ui_button_t btn_load;
    char buffer[4096];
    char filename[MYFS_MAX_NAME];
} notepad_state_t;

/* ── Draw callback ───────────────────────────────────────── */
static void notepad_draw(window_t* win, int cx, int cy, int cw, int ch) {
    (void)cw;  /* unused */
    notepad_state_t* state = (notepad_state_t*)win->app_data;
    if (!state) return;
    
    /* Clear client area */
    gfx_rect_fill(cx, cy, cw, ch, COLOR_WINBG);
    
    /* Draw textarea with offset */
    ui_textarea_t offset_area = state->textarea;
    offset_area.x += cx;
    offset_area.y += cy;
    ui_textarea_draw(&offset_area);
    
    /* Draw buttons with offset */
    ui_button_t btn_save = state->btn_save;
    btn_save.x += cx;
    btn_save.y += cy;
    ui_button_draw(&btn_save);
    
    ui_button_t btn_load = state->btn_load;
    btn_load.x += cx;
    btn_load.y += cy;
    ui_button_draw(&btn_load);
}

/* ── Event callback ───────────────────────────────────────── */
static void notepad_event(window_t* win, const event_t* e) {
    notepad_state_t* state = (notepad_state_t*)win->app_data;
    if (!state) return;
    
    /* Handle textarea events */
    uint32_t old_pos = state->textarea.pos;
    ui_textarea_event(&state->textarea, e);
    if (state->textarea.pos != old_pos) {
        wm_invalidate(win);  /* Mark window dirty after text input */
    }
    
    /* Handle button events */
    ui_button_event(&state->btn_save, e, 0, 0);
    ui_button_event(&state->btn_load, e, 0, 0);
    
    if (state->btn_save.clicked) {
        state->btn_save.clicked = 0;
        if (strlen(state->filename) > 0 && state->textarea.pos > 0) {
            uint32_t bytes_to_save = state->textarea.pos;
            int saved = 0;
            
            /* 1. Try to update existing file in ZFSX */
            if (zfsx_is_mounted() && zfsx_find_in_dir(ZFSX_ROOT_ID, state->filename) >= 0) {
                int obj_id = zfsx_find_in_dir(ZFSX_ROOT_ID, state->filename);
                zfsx_write(obj_id, state->buffer, bytes_to_save);
                saved = 1;
            } 
            /* 2. Try to update existing file in MyFS (check by reading 1 byte) */
            else if (!saved) {
                 uint8_t tmp;
                 if (myfs_read(state->filename, &tmp, 1) >= 0) {
                     myfs_write(state->filename, (uint8_t*)state->buffer, bytes_to_save);
                     saved = 1;
                 }
            }

            /* 3. If new file, prefer ZFSX, then MyFS */
            if (!saved) {
                if (zfsx_is_mounted()) {
                    int obj_id = zfsx_create(ZFSX_ROOT_ID, state->filename, ZFSX_TYPE_FILE);
                    if (obj_id >= 0) zfsx_write(obj_id, state->buffer, bytes_to_save);
                } else {
                    myfs_write(state->filename, (uint8_t*)state->buffer, bytes_to_save);
                }
            }
        }
        wm_invalidate(win);
    }
    
    if (state->btn_load.clicked) {
        state->btn_load.clicked = 0;
        memset(state->buffer, 0, sizeof(state->buffer));
        int bytes_read = -1;
        if (strlen(state->filename) > 0) {
            /* Try all filesystems until found */
            if (zfsx_is_mounted()) {
                bytes_read = zfsx_read_file(state->filename, (uint8_t*)state->buffer, sizeof(state->buffer) - 1);
            }
            if (bytes_read < 0 && fat32_is_mounted()) {
                bytes_read = fat32_read_file(state->filename, (uint8_t*)state->buffer, sizeof(state->buffer) - 1);
            }
            if (bytes_read < 0) {
                bytes_read = myfs_read(state->filename, (uint8_t*)state->buffer, sizeof(state->buffer) - 1);
            }
            if (bytes_read > 0) {
                state->buffer[bytes_read] = '\0';
                state->textarea.pos = bytes_read;
            }
        }
        wm_invalidate(win);
    }
}

/* ── Launch function ───────────────────────────────────────── */
void notepad_launch(void) {
    notepad_state_t* state = (notepad_state_t*)kmalloc(sizeof(notepad_state_t));
    if (!state) return;
    
    memset(state, 0, sizeof(notepad_state_t));
    memset(state->buffer, 0, sizeof(state->buffer));
    strcpy(state->filename, "note.txt");
    
    state->textarea = ui_textarea_create(10, 10, 380, 300, state->buffer, sizeof(state->buffer));
    state->btn_save = ui_button_create(10, 320, 80, 24, "Save");
    state->btn_load = ui_button_create(100, 320, 80, 24, "Load");
    
    window_t* win = wm_create("Notepad", 200, 100, 400, 360, notepad_draw, notepad_event);
    if (win) {
        win->app_data = state;
    }
}