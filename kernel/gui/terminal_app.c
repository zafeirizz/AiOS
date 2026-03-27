#include <stdint.h>
#include "../../include/wm.h"
#include "../../include/gfx.h"
#include "../../include/event.h"
#include "../../include/string.h"
#include "../../include/shell.h"
#include "../../include/shell_gui.h"
#include "../../include/fb.h"
#include "../../include/heap.h"

#define TERM_COLS  80
#define TERM_ROWS  24
#define TERM_PAD   4

/* ── VT100 color table (16 ANSI colors) ────────────────── */
static const uint32_t ansi_colors[16] = {
    RGB(8,10,18),     RGB(200,50,50),    RGB(0,200,120),  RGB(200,150,0),
    RGB(0,80,200),    RGB(150,0,200),    RGB(0,200,180),  RGB(160,180,170),
    RGB(40,50,60),    RGB(255,80,80),    RGB(0,255,160),  RGB(255,200,0),
    RGB(80,140,255),  RGB(200,80,255),   RGB(0,255,220),  RGB(210,230,220),
};
#define VT_DEFAULT_FG  14   /* bright teal */
#define VT_DEFAULT_BG   0   /* near-black */

typedef struct { char c; uint8_t fg; uint8_t bg; } cell_t;

typedef struct {
    cell_t   cells[TERM_ROWS][TERM_COLS];
    int      cur_row, cur_col;
    uint8_t  cur_fg, cur_bg;

    /* VT100 escape state */
    int      esc_state;    /* 0=normal 1=got-ESC 2=in-CSI */
    char     esc_buf[16];
    int      esc_len;

    int      dirty;
} term_state_t;

static term_state_t* ts = (void*)0;

/* ── Helpers ──────────────────────────────────────────── */
static void term_clear_row(int r) {
    for (int c=0;c<TERM_COLS;c++)
        ts->cells[r][c] = (cell_t){' ', ts->cur_fg, ts->cur_bg};
}

static void term_scroll(void) {
    for (int r=0;r<TERM_ROWS-1;r++)
        for (int c=0;c<TERM_COLS;c++)
            ts->cells[r][c] = ts->cells[r+1][c];
    term_clear_row(TERM_ROWS-1);
    ts->cur_row = TERM_ROWS-1;
}

/* Parse simple CSI sequences: ESC [ Pm m  (SGR color) */
static void term_handle_csi(void) {
    /* ts->esc_buf contains chars between '[' and final byte */
    char* p = ts->esc_buf;
    /* Final byte already stripped — it's in esc_buf[esc_len-1] */
    char final = ts->esc_buf[ts->esc_len > 0 ? ts->esc_len-1 : 0];

    if (final == 'm') {
        /* SGR: parse semicolon-separated params */
        int params[8]; int np=0;
        int v=0; int has_v=0;
        for (int i=0; i<ts->esc_len-1; i++){
            char c=p[i];
            if (c>='0'&&c<='9'){ v=v*10+(c-'0'); has_v=1; }
            else if (c==';'){
                if (np<8) params[np++] = has_v ? v : 0;
                v=0; has_v=0;
            }
        }
        if (np<8) params[np++] = has_v ? v : 0;
        if (np==0) params[np++]=0;

        for (int i=0;i<np;i++){
            int n=params[i];
            if (n==0)         { ts->cur_fg=VT_DEFAULT_FG; ts->cur_bg=VT_DEFAULT_BG; }
            else if (n==1)    { /* bold: bump fg to bright */ if(ts->cur_fg<8) ts->cur_fg+=8; }
            else if (n>=30&&n<=37) ts->cur_fg=(uint8_t)(n-30);
            else if (n==39)   ts->cur_fg=VT_DEFAULT_FG;
            else if (n>=40&&n<=47) ts->cur_bg=(uint8_t)(n-40);
            else if (n==49)   ts->cur_bg=VT_DEFAULT_BG;
            else if (n>=90&&n<=97) ts->cur_fg=(uint8_t)(n-90+8);
            else if (n>=100&&n<=107) ts->cur_bg=(uint8_t)(n-100+8);
        }
    } else if (final=='J') {
        /* ED: erase display */
        for (int r=0;r<TERM_ROWS;r++) term_clear_row(r);
        ts->cur_row=0; ts->cur_col=0;
    } else if (final=='K') {
        /* EL: erase line */
        for (int c=ts->cur_col;c<TERM_COLS;c++)
            ts->cells[ts->cur_row][c]=(cell_t){' ',ts->cur_fg,ts->cur_bg};
    } else if (final=='H' || final=='f') {
        /* CUP: cursor position */
        int row=0,col=0;
        const char* pp=p; int pv=0,pc=0;
        while(*pp&&*pp!=';'&&*pp!=final){if(*pp>='0'&&*pp<='9')pv=pv*10+(*pp-'0');pp++;}
        row=pv; pv=0;
        if (*pp==';'){pp++;while(*pp&&*pp!=final){if(*pp>='0'&&*pp<='9')pv=pv*10+(*pp-'0');pp++;}}
        col=pv;
        (void)pc;
        ts->cur_row=(row>0?row-1:0); if(ts->cur_row>=TERM_ROWS)ts->cur_row=TERM_ROWS-1;
        ts->cur_col=(col>0?col-1:0); if(ts->cur_col>=TERM_COLS)ts->cur_col=TERM_COLS-1;
    }
}

/* ── Main putchar — called by shell via callback ──────── */
void term_putchar(char ch) {
    if (!ts) return;

    /* ESC state machine */
    if (ts->esc_state == 1) {
        if (ch == '[') { ts->esc_state=2; ts->esc_len=0; }
        else ts->esc_state=0;
        return;
    }
    if (ts->esc_state == 2) {
        if (ts->esc_len < 14) ts->esc_buf[ts->esc_len++]=ch;
        ts->esc_buf[ts->esc_len]='\0';
        /* Final byte is alpha */
        if ((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')) {
            term_handle_csi();
            ts->esc_state=0;
        }
        return;
    }

    if (ch == 0x1B) { ts->esc_state=1; return; }

    if (ch == '\n') {
        ts->cur_col = 0;
        ts->cur_row++;
        if (ts->cur_row >= TERM_ROWS) term_scroll();
        ts->dirty = 1; return;
    }
    if (ch == '\r') { ts->cur_col=0; ts->dirty=1; return; }
    if (ch == '\b') {
        if (ts->cur_col > 0) ts->cur_col--;
        ts->cells[ts->cur_row][ts->cur_col]=(cell_t){' ',ts->cur_fg,ts->cur_bg};
        ts->dirty=1; return;
    }
    if (ch == '\t') {
        int next=((ts->cur_col/8)+1)*8;
        if (next>=TERM_COLS) next=TERM_COLS-1;
        while(ts->cur_col<next)
            ts->cells[ts->cur_row][ts->cur_col++]=(cell_t){' ',ts->cur_fg,ts->cur_bg};
        ts->dirty=1; return;
    }

    if (ts->cur_col >= TERM_COLS) {
        ts->cur_col=0; ts->cur_row++;
        if (ts->cur_row >= TERM_ROWS) term_scroll();
    }
    ts->cells[ts->cur_row][ts->cur_col++]=(cell_t){ch, ts->cur_fg, ts->cur_bg};
    ts->dirty=1;
}

/* ── Draw ─────────────────────────────────────────────── */
static void term_draw(window_t* win, int x, int y, int w, int h) {
    (void)win; (void)w; (void)h;
    if (!ts) return;

    gfx_rect_fill(x, y,
                  TERM_COLS*GFX_CHAR_W + TERM_PAD*2,
                  TERM_ROWS*GFX_CHAR_H + TERM_PAD*2,
                  ansi_colors[VT_DEFAULT_BG]);

    for (int r=0;r<TERM_ROWS;r++) {
        for (int c=0;c<TERM_COLS;c++) {
            cell_t* cell=&ts->cells[r][c];
            int px=x+TERM_PAD+c*GFX_CHAR_W;
            int py=y+TERM_PAD+r*GFX_CHAR_H;
            uint32_t fg=ansi_colors[cell->fg & 0xF];
            uint32_t bg=ansi_colors[cell->bg & 0xF];
            gfx_text_bg(px,py,(char[]){cell->c,0}, fg, bg);
        }
    }

    /* Blinking cursor (always draw) */
    if (ts->cur_row<TERM_ROWS && ts->cur_col<TERM_COLS) {
        int px=x+TERM_PAD+ts->cur_col*GFX_CHAR_W;
        int py=y+TERM_PAD+ts->cur_row*GFX_CHAR_H;
        gfx_rect_fill(px, py+GFX_CHAR_H-2, GFX_CHAR_W, 2, ansi_colors[VT_DEFAULT_FG]);
    }
    ts->dirty=0;
}

/* ── Event ────────────────────────────────────────────── */
static void term_event(window_t* win, const event_t* e) {
    if (e->type == EVT_KEY_DOWN && e->key.c != 0) {
        shell_handle_input(e->key.c);
        wm_invalidate(win);
    }
}

/* ── Launch ───────────────────────────────────────────── */
void wm_launch_terminal(void) {
    if (!ts) {
        ts = (term_state_t*)kmalloc(sizeof(term_state_t));
        if (!ts) return;
    }
    memset(ts, 0, sizeof(term_state_t));
    ts->cur_fg = VT_DEFAULT_FG;
    ts->cur_bg = VT_DEFAULT_BG;
    for (int r=0;r<TERM_ROWS;r++) term_clear_row(r);

    int ww = TERM_COLS*GFX_CHAR_W + TERM_PAD*2 + 2*WM_BORDER;
    int wh = TERM_ROWS*GFX_CHAR_H + TERM_PAD*2 + WM_TITLE_H + 2*WM_BORDER;

    window_t* win = wm_create("Terminal", 40, 40, ww, wh,
                              term_draw, term_event);
    (void)win;
    shell_init_gui(term_putchar);
}
