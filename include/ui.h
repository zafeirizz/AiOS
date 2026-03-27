#ifndef UI_H
#define UI_H

#include <stdint.h>
#include "event.h"
#include "gfx.h"

/* ── UI Component System ──────────────────────────────────────
 * Reusable widgets for building apps. All draw to back buffer.
 * ───────────────────────────────────────────────────────────── */

/* ── Button Component ──────────────────────────────────── */
typedef struct {
    int x, y, w, h;
    char label[64];
    int hovered;
    int pressed;
    int clicked;  /* set by ui_button_event() if clicked this frame */
} ui_button_t;

ui_button_t ui_button_create(int x, int y, int w, int h, const char* label);
void ui_button_draw(const ui_button_t* btn);
void ui_button_event(ui_button_t* btn, const event_t* e, int cx, int cy);


/* ── List Component ───────────────────────────────────── */
typedef struct {
    int x, y, w, h;
    int item_h;          /* height per item */
    int scroll_y;        /* scroll offset */
    int selected;        /* selected item index, -1 = none */
    int clicked;         /* set if item clicked */
    int clicked_idx;     /* which item */
    
    const char** items;  /* array of item strings */
    int count;          /* number of items */
} ui_list_t;

ui_list_t ui_list_create(int x, int y, int w, int h, int item_h);
void ui_list_set_items(ui_list_t* list, const char** items, int count);
void ui_list_draw(const ui_list_t* list);
void ui_list_event(ui_list_t* list, const event_t* e, int cx, int cy);


/* ── Textbox Component ────────────────────────────────– */
typedef struct {
    int x, y, w, h;
    char* buffer;       /* text buffer */
    uint32_t buf_size;  /* buffer capacity */
    uint32_t pos;       /* cursor position */
    int focused;
} ui_textbox_t;

ui_textbox_t ui_textbox_create(int x, int y, int w, int h, char* buf, uint32_t size);
void ui_textbox_draw(const ui_textbox_t* box);
void ui_textbox_event(ui_textbox_t* box, const event_t* e);


/* ── Textarea Component ────────────────────────────────– */
typedef struct {
    int x, y, w, h;
    char* buffer;       /* text buffer */
    uint32_t buf_size;  /* buffer capacity */
    uint32_t pos;       /* cursor position in buffer */
    int scroll_y;       /* scroll offset in lines */
    int focused;
} ui_textarea_t;

ui_textarea_t ui_textarea_create(int x, int y, int w, int h, char* buf, uint32_t size);
void ui_textarea_draw(const ui_textarea_t* area);
void ui_textarea_event(ui_textarea_t* area, const event_t* e);


/* ── Label Component ──────────────────────────────────– */
typedef struct {
    int x, y;
    char text[256];
    uint32_t color;
} ui_label_t;

ui_label_t ui_label_create(int x, int y, const char* text, uint32_t color);
void ui_label_draw(const ui_label_t* label);


/* ── Separator ──────────────────────────────────────– */
void ui_draw_separator(int x, int y, int w, uint32_t color);
void ui_draw_spacer(int* y, int h);

#endif
