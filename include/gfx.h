#ifndef GFX_H
#define GFX_H

#include <stdint.h>

/* ── Color helpers ────────────────────────────────────── */
#define RGB(r,g,b)     ((uint32_t)(((r)<<16)|((g)<<8)|(b)))
#define RGBA(r,g,b,a)  ((uint32_t)(((a)<<24)|((r)<<16)|((g)<<8)|(b)))

/* ── QifshaOS "Terminal-Neon" palette ─────────────────── *
 *  Deep navy/charcoal base, electric cyan accent,         *
 *  amber highlights, monospace grid aesthetic             *
 * ─────────────────────────────────────────────────────── */
#define COLOR_BLACK    RGB(0,0,0)
#define COLOR_WHITE    RGB(255,255,255)
#define COLOR_RED      RGB(255,70,70)
#define COLOR_GREEN    RGB(0,230,120)
#define COLOR_BLUE     RGB(30,100,220)
#define COLOR_CYAN     RGB(0,220,200)
#define COLOR_YELLOW   RGB(255,200,0)
#define COLOR_GRAY     RGB(100,110,130)
#define COLOR_DGRAY    RGB(14,18,28)
#define COLOR_LGRAY    RGB(160,170,190)

/* Desktop / chrome */
#define COLOR_BG        RGB(8,10,18)      /* near-black navy          */
#define COLOR_TITLEBAR  RGB(16,20,32)     /* window header bg         */
#define COLOR_WINBG     RGB(12,15,24)     /* window client area       */
#define COLOR_BORDER    RGB(0,180,160)    /* teal border              */
#define COLOR_BORDER_DIM RGB(0,60,55)     /* unfocused border         */
#define COLOR_ACCENT    RGB(0,220,200)    /* electric teal            */
#define COLOR_ACCENT2   RGB(255,160,0)    /* amber — secondary accent */
#define COLOR_ACCENT3   RGB(180,0,255)    /* purple — tertiary        */
#define COLOR_TEXT_D    RGB(200,230,220)  /* text on dark bg          */
#define COLOR_TEXT_L    RGB(150,190,180)  /* dimmer text              */
#define COLOR_TEXT_HI   RGB(0,255,200)    /* highlighted text         */

/* ── Clipping rectangle ───────────────────────────────── */
void gfx_set_clip(int x, int y, int w, int h);
void gfx_clear_clip(void);

/* ── Primitives ───────────────────────────────────────── */
void gfx_pixel(int x, int y, uint32_t color);
void gfx_hline(int x, int y, int w, uint32_t color);
void gfx_vline(int x, int y, int h, uint32_t color);
void gfx_rect(int x, int y, int w, int h, uint32_t color);
void gfx_rect_fill(int x, int y, int w, int h, uint32_t color);
void gfx_rect_rounded(int x, int y, int w, int h, int r, uint32_t color);
void gfx_rect_rounded_fill(int x, int y, int w, int h, int r, uint32_t color);
void gfx_line(int x0, int y0, int x1, int y1, uint32_t color);
void gfx_circle_fill(int cx, int cy, int r, uint32_t color);

/* ── QifshaOS-specific decorative primitives ─────────── */
/* Corner bracket: draws only the corners of a rect (not full border) */
void gfx_corner_brackets(int x, int y, int w, int h, int sz, uint32_t color);
/* Dashed horizontal line */
void gfx_dashed_hline(int x, int y, int w, int dash, int gap, uint32_t color);
/* Scanline effect: darken every other row in a rect */
void gfx_scanlines(int x, int y, int w, int h);
/* Draw a right-angled "tab" header bar with notch cut */
void gfx_tab_bar(int x, int y, int w, int h, uint32_t bg, uint32_t border);

/* ── Text ─────────────────────────────────────────────── */
void gfx_text(int x, int y, const char* str, uint32_t fg);
void gfx_text_bg(int x, int y, const char* str, uint32_t fg, uint32_t bg);
int  gfx_text_width(const char* str);
#define GFX_CHAR_W  8
#define GFX_CHAR_H  16

/* ── Blit ─────────────────────────────────────────────── */
void gfx_blit(int dx, int dy, int w, int h,
               const uint32_t* src, int src_stride);

#endif
