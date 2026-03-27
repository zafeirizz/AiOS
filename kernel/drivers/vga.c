#include <stddef.h>
#include <stdint.h>
#include "../../include/vga.h"
#include "../../include/font8x16.h"
#include "../../include/fb.h"
#include "../../include/gfx.h"

// Extern framebuffer variables
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint32_t* fb_back;

// Extern fb functions
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

static uint16_t* const vga_buf = (uint16_t*)0xB8000;

/* Standard VGA 16-color palette (RGB 0-63) */
static const uint8_t vga_palette[16*3] = {
    0,0,0,      0,0,42,     0,42,0,     0,42,42,
    42,0,0,     42,0,42,    42,21,0,    42,42,42,
    21,21,21,   21,21,63,   21,63,21,   21,63,63,
    63,21,21,   63,21,63,   63,63,21,   63,63,63
};

static int     cur_row;
static int     cur_col;
static uint8_t cur_color;

static int tty_mode = 0;
static uint32_t tty_cx = 0, tty_cy = 0;
static uint32_t tty_bg = 0x000000;

typedef struct {
    uint8_t misc;
    uint8_t sequencer[5];
    uint8_t crtc[25];
    uint8_t graphics[9];
    uint8_t attribute[21];
} vga_regs_t;

/* ── I/O ──────────────────────────────────────────────── */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* ── Helpers ──────────────────────────────────────────── */

static inline uint16_t make_entry(char c, uint8_t color) {
    return (uint16_t)(unsigned char)c | ((uint16_t)color << 8);
}

static void hw_cursor(int row, int col) {
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/* ── Scroll one line up ───────────────────────────────── */

static void scroll(void) {
    for (int y = 1; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            vga_buf[(y-1)*VGA_WIDTH + x] = vga_buf[y*VGA_WIDTH + x];
    for (int x = 0; x < VGA_WIDTH; x++)
        vga_buf[(VGA_HEIGHT-1)*VGA_WIDTH + x] = make_entry(' ', cur_color);
    cur_row = VGA_HEIGHT - 1;
}

/* ── Init ─────────────────────────────────────────────── */

void vga_init(void) {
    cur_row = cur_col = 0;
    cur_color = VGA_COLOR(VGA_WHITE, VGA_BLACK);
    for (int y = 0; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            vga_buf[y*VGA_WIDTH + x] = make_entry(' ', cur_color);
    hw_cursor(0, 0);
}

/* ── Color ────────────────────────────────────────────── */

void terminal_setcolor(uint8_t color) { cur_color = color; }
int  terminal_get_row(void)           { return cur_row; }
int  terminal_get_col(void)           { return cur_col; }

/* Helper to convert VGA palette index to RGB */
static uint32_t vga_pal_to_rgb(uint8_t index) {
    index &= 0x0F;
    uint32_t r = vga_palette[index * 3] * 4;
    uint32_t g = vga_palette[index * 3 + 1] * 4;
    uint32_t b = vga_palette[index * 3 + 2] * 4;
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

/* ── Direct cell access (for editor) ─────────────────── */

void terminal_put_at(int x, int y, char c, uint8_t color) {
    if (tty_mode) {
        if (x < 0 || x >= (int)(fb_width / 8) || y < 0 || y >= (int)(fb_height / 16)) return;
        uint32_t fg = vga_pal_to_rgb(color);
        uint32_t bg = vga_pal_to_rgb(color >> 4);
        char s[2] = {c, 0};
        gfx_text_bg(x * 8, y * 16, s, fg, bg);
        return;
    }
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) return;
    vga_buf[y*VGA_WIDTH + x] = make_entry(c, color);
}

void terminal_write_at(int x, int y, const char* str, uint8_t color) {
    for (int i = 0; str[i] && x+i < VGA_WIDTH; i++)
        terminal_put_at(x+i, y, str[i], color);
}

void terminal_clear_line(int y) {
    if (tty_mode) {
        if (y < 0 || y >= (int)(fb_height / 16)) return;
        uint32_t bg = vga_pal_to_rgb(cur_color >> 4);
        fb_fill_rect(0, y * 16, fb_width, 16, bg);
        return;
    }
    if (y < 0 || y >= VGA_HEIGHT) return;
    for (int x = 0; x < VGA_WIDTH; x++)
        vga_buf[y*VGA_WIDTH + x] = make_entry(' ', cur_color);
}

void terminal_move_cursor(int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= VGA_WIDTH)  x = VGA_WIDTH  - 1;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;
    
    if (tty_mode) {
        tty_cx = x * 8;
        tty_cy = y * 16;
        /* Draw cursor underline and flip to show updates */
        fb_fill_rect(tty_cx, tty_cy + 14, 8, 2, 0xFFFFFFFF);
        fb_flip();
        return;
    }
    
    cur_col = x;
    cur_row = y;
    hw_cursor(y, x);
}

/* ── Streaming putchar (for shell/kernel messages) ────── */

void terminal_putchar(char c) {
    if (tty_mode) {
        /* Convert current VGA color to RGB for TTY output */
        uint32_t fg = vga_pal_to_rgb(cur_color);
        uint32_t bg = vga_pal_to_rgb(cur_color >> 4);
        
        if (c == '\n') {
            tty_cx = 0;
            tty_cy += 16;
            if (tty_cy >= fb_height) {
                tty_cy = fb_height - 16;
                // Scroll the back buffer
                for (uint32_t y = 16; y < fb_height; y++) {
                    for (uint32_t x = 0; x < fb_width; x++) {
                        fb_back[y * (fb_pitch / 4) + x] = fb_back[(y - 16) * (fb_pitch / 4) + x];
                    }
                }
                for (uint32_t x = 0; x < fb_width; x++) {
                    fb_back[(fb_height - 16) * (fb_pitch / 4) + x] = bg;
                }
            }
        } else if (c == '\b') {
            if (tty_cx >= 8) {
                tty_cx -= 8;
                fb_fill_rect(tty_cx, tty_cy, 8, 16, bg);
            }
        } else if (c == '\t') {
            tty_cx = ((tty_cx / 64) + 1) * 64;
            if (tty_cx >= fb_width) {
                tty_cx = 0;
                tty_cy += 16;
                if (tty_cy >= fb_height) {
                    tty_cy = fb_height - 16;
                    // Scroll
                    for (uint32_t y = 16; y < fb_height; y++) {
                        for (uint32_t x = 0; x < fb_width; x++) {
                            fb_back[y * (fb_pitch / 4) + x] = fb_back[(y - 16) * (fb_pitch / 4) + x];
                        }
                    }
                    for (uint32_t x = 0; x < fb_width; x++) {
                        fb_back[(fb_height - 16) * (fb_pitch / 4) + x] = bg;
                    }
                }
            }
        } else {
            char str[2] = {c, 0};
            gfx_text_bg(tty_cx, tty_cy, str, fg, bg);
            tty_cx += 8;
            if (tty_cx >= fb_width) {
                tty_cx = 0;
                tty_cy += 16;
                if (tty_cy >= fb_height) {
                    tty_cy = fb_height - 16;
                    // Scroll
                    for (uint32_t y = 16; y < fb_height; y++) {
                        for (uint32_t x = 0; x < fb_width; x++) {
                            fb_back[y * (fb_pitch / 4) + x] = fb_back[(y - 16) * (fb_pitch / 4) + x];
                        }
                    }
                    for (uint32_t x = 0; x < fb_width; x++) {
                        fb_back[(fb_height - 16) * (fb_pitch / 4) + x] = bg;
                    }
                }
            }
        }
        fb_flip();
    } else {
        if (c == '\n') {
            cur_col = 0;
            if (++cur_row >= VGA_HEIGHT) scroll();
            hw_cursor(cur_row, cur_col);
            return;
        }
        if (c == '\b') {
            if (cur_col > 0) cur_col--;
            else if (cur_row > 0) { cur_row--; cur_col = VGA_WIDTH-1; }
            vga_buf[cur_row*VGA_WIDTH + cur_col] = make_entry(' ', cur_color);
            hw_cursor(cur_row, cur_col);
            return;
        }
        if (c == '\t') {
            do { terminal_putchar(' '); } while (cur_col % 4);
            return;
        }
        vga_buf[cur_row*VGA_WIDTH + cur_col] = make_entry(c, cur_color);
        if (++cur_col >= VGA_WIDTH) {
            cur_col = 0;
            if (++cur_row >= VGA_HEIGHT) scroll();
        }
        hw_cursor(cur_row, cur_col);
    }
}

void terminal_write(const char* s)     { for (; *s; s++) terminal_putchar(*s); }
void terminal_writeline(const char* s) { terminal_write(s); terminal_putchar('\n'); }

void terminal_write_hex(uint32_t v) {
    const char* d = "0123456789ABCDEF";
    char buf[11]; buf[0]='0'; buf[1]='x'; buf[10]=0;
    for (int i = 9; i >= 2; i--) { buf[i] = d[v&0xF]; v >>= 4; }
    terminal_write(buf);
}

void terminal_write_dec(uint32_t v) {
    if (!v) { terminal_putchar('0'); return; }
    char buf[12]; int i = 11; buf[i] = 0;
    while (v) { buf[--i] = '0' + v%10; v /= 10; }
    terminal_write(&buf[i]);
}

void terminal_clear(void) {
    if (tty_mode) {
        uint32_t bg = vga_pal_to_rgb(cur_color >> 4);
        tty_cx = 0;
        tty_cy = 0;
        fb_clear(bg);
        fb_flip();
    } else {
        cur_row = cur_col = 0;
        for (int y = 0; y < VGA_HEIGHT; y++)
            for (int x = 0; x < VGA_WIDTH; x++)
                vga_buf[y*VGA_WIDTH + x] = make_entry(' ', cur_color);
        hw_cursor(0, 0);
    }
}

void vga_enter_text_mode(void) {
    tty_mode = 1;
    tty_cx = 0;
    tty_cy = 0;
    // Clear screen
    fb_clear(tty_bg);
    fb_flip();
}

void vga_exit_text_mode(void) {
    tty_mode = 0;
}
