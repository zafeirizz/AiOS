#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

#define VGA_COLOR(fg, bg) ((uint8_t)((bg) << 4 | (fg)))

typedef enum {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE
} vga_color_t;

void vga_init(void);
void vga_enter_text_mode(void);
void vga_exit_text_mode(void);
void terminal_putchar(char c);
void terminal_write(const char* str);
void terminal_writeline(const char* str);
void terminal_write_hex(uint32_t val);
void terminal_write_dec(uint32_t val);
void terminal_setcolor(uint8_t color);
void terminal_clear(void);

/* Phase 3: precise cursor control for the editor */
void terminal_move_cursor(int x, int y);
void terminal_clear_line(int y);
void terminal_put_at(int x, int y, char c, uint8_t color);
void terminal_write_at(int x, int y, const char* str, uint8_t color);
int  terminal_get_row(void);
int  terminal_get_col(void);

#endif
