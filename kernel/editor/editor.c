#include <stdint.h>
#include "../../include/editor.h"
#include "../../include/vga.h"
#include "../../include/keyboard.h"
#include "../../include/string.h"
#include "../../include/myfs.h"

/* ── Layout ───────────────────────────────────────────── */

#define ED_ROWS      (VGA_HEIGHT - 2)   /* rows reserved for text (23) */
#define ED_COLS      VGA_WIDTH          /* 80 */
#define MAX_LINES    200
#define LINE_LEN     (ED_COLS + 1)      /* +1 for null terminator */

/* ── Colours ──────────────────────────────────────────── */

#define COL_TEXT     VGA_COLOR(VGA_WHITE,      VGA_BLACK)
#define COL_STATUS   VGA_COLOR(VGA_BLACK,      VGA_LIGHT_GREY)
#define COL_TITLE    VGA_COLOR(VGA_BLACK,      VGA_CYAN)
#define COL_MODIFIED VGA_COLOR(VGA_YELLOW,     VGA_BLACK)
#define COL_CURSOR   VGA_COLOR(VGA_BLACK,      VGA_WHITE)

/* ── State ────────────────────────────────────────────── */

static char  lines[MAX_LINES][LINE_LEN];
static int   line_len[MAX_LINES];   /* length of each line (no null) */
static int   num_lines;

static int   cx, cy;                /* cursor: column, line index */
static int   view_top;              /* first visible line index */

static int   modified;
static char  filename[MYFS_MAX_NAME];

/* ── Helpers ──────────────────────────────────────────── */

static void clamp_cx(void) {
    if (cx > line_len[cy]) cx = line_len[cy];
    if (cx < 0)            cx = 0;
}

/* Insert character at (cx, cy) */
static void insert_char(char c) {
    if (line_len[cy] >= ED_COLS - 1) return;   /* line full */
    /* Shift right */
    for (int i = line_len[cy]; i > cx; i--)
        lines[cy][i] = lines[cy][i-1];
    lines[cy][cx]       = c;
    lines[cy][line_len[cy]+1] = '\0';
    line_len[cy]++;
    cx++;
    modified = 1;
}

/* Delete char before cursor (backspace) */
static void delete_char(void) {
    if (cx > 0) {
        for (int i = cx-1; i < line_len[cy]-1; i++)
            lines[cy][i] = lines[cy][i+1];
        lines[cy][line_len[cy]-1] = '\0';
        line_len[cy]--;
        cx--;
        modified = 1;
        return;
    }
    /* cx == 0: merge with previous line */
    if (cy == 0) return;
    int prev_len = line_len[cy-1];
    int cur_len  = line_len[cy];
    if (prev_len + cur_len < ED_COLS - 1) {
        /* Append current line to previous */
        memcpy(lines[cy-1] + prev_len, lines[cy], cur_len + 1);
        line_len[cy-1] = prev_len + cur_len;
        /* Remove current line by shifting up */
        for (int i = cy; i < num_lines-1; i++) {
            memcpy(lines[i], lines[i+1], LINE_LEN);
            line_len[i] = line_len[i+1];
        }
        memset(lines[num_lines-1], 0, LINE_LEN);
        line_len[num_lines-1] = 0;
        num_lines--;
        cy--;
        cx = prev_len;
        modified = 1;
    }
}

/* Insert newline at cursor position */
static void insert_newline(void) {
    if (num_lines >= MAX_LINES) return;
    /* Shift lines down */
    for (int i = num_lines; i > cy+1; i--) {
        memcpy(lines[i], lines[i-1], LINE_LEN);
        line_len[i] = line_len[i-1];
    }
    /* Split current line at cx */
    int rest = line_len[cy] - cx;
    memcpy(lines[cy+1], lines[cy] + cx, rest);
    lines[cy+1][rest] = '\0';
    line_len[cy+1]    = rest;
    lines[cy][cx]     = '\0';
    line_len[cy]      = cx;
    num_lines++;
    cy++;
    cx = 0;
    modified = 1;
}

/* ── Rendering ────────────────────────────────────────── */

static void draw_title_bar(void) {
    uint8_t col = COL_TITLE;
    /* Clear top row */
    for (int x = 0; x < VGA_WIDTH; x++)
        terminal_put_at(x, 0, ' ', col);

    const char* title = "myOS Editor";
    int tx = (VGA_WIDTH - (int)strlen(title)) / 2;
    terminal_write_at(tx, 0, title, col);

    if (filename[0]) {
        terminal_write_at(1, 0, filename, col);
    } else {
        terminal_write_at(1, 0, "[new file]", col);
    }
    if (modified)
        terminal_put_at(VGA_WIDTH-2, 0, '*', VGA_COLOR(VGA_YELLOW, VGA_CYAN));
}

static void draw_status_bar(void) {
    uint8_t col = COL_STATUS;
    for (int x = 0; x < VGA_WIDTH; x++)
        terminal_put_at(x, VGA_HEIGHT-1, ' ', col);

    /* Left: key hints */
    terminal_write_at(0, VGA_HEIGHT-1,
        "^S Save  ^Q Quit  ^N New  Arrows Move", col);

    /* Right: cursor position */
    char pos[24];
    /* Build "Ln XXX Col XX" manually */
    int pi = 0;
    const char* ln = "Ln ";
    for (int i = 0; ln[i]; i++) pos[pi++] = ln[i];
    uint32_t row_n = cy + 1;
    if (row_n >= 100) pos[pi++] = '0' + (row_n/100)%10;
    if (row_n >= 10)  pos[pi++] = '0' + (row_n/10)%10;
    pos[pi++] = '0' + row_n%10;
    const char* cl = " Col ";
    for (int i = 0; cl[i]; i++) pos[pi++] = cl[i];
    uint32_t col_n = cx + 1;
    if (col_n >= 10) pos[pi++] = '0' + (col_n/10)%10;
    pos[pi++] = '0' + col_n%10;
    pos[pi] = '\0';

    int px = VGA_WIDTH - pi - 1;
    terminal_write_at(px, VGA_HEIGHT-1, pos, col);
}

static void draw_text_area(void) {
    uint8_t norm = COL_TEXT;
    for (int screen_row = 0; screen_row < ED_ROWS; screen_row++) {
        int line_idx = view_top + screen_row;
        int vga_row  = screen_row + 1;    /* +1 for title bar */

        /* Clear the line first */
        for (int x = 0; x < VGA_WIDTH; x++)
            terminal_put_at(x, vga_row, ' ', norm);

        if (line_idx >= num_lines) {
            /* Tilde for lines past end of file (like vim) */
            terminal_put_at(0, vga_row, '~', VGA_COLOR(VGA_DARK_GREY, VGA_BLACK));
            continue;
        }

        /* Draw the line text */
        for (int x = 0; x < line_len[line_idx] && x < VGA_WIDTH; x++)
            terminal_put_at(x, vga_row, lines[line_idx][x], norm);
    }
}

static void place_hw_cursor(void) {
    int screen_row = (cy - view_top) + 1;   /* +1 for title bar */
    terminal_move_cursor(cx, screen_row);
}

static void redraw(void) {
    draw_title_bar();
    draw_text_area();
    draw_status_bar();
    place_hw_cursor();
}

/* ── Scrolling ────────────────────────────────────────── */

static void scroll_to_cursor(void) {
    if (cy < view_top)
        view_top = cy;
    if (cy >= view_top + ED_ROWS)
        view_top = cy - ED_ROWS + 1;
}

/* ── Save / Load ──────────────────────────────────────── */

static void show_message(const char* msg, uint8_t col) {
    for (int x = 0; x < VGA_WIDTH; x++)
        terminal_put_at(x, VGA_HEIGHT-1, ' ', col);
    terminal_write_at(0, VGA_HEIGHT-1, msg, col);
}

static void save_file(void) {
    if (!filename[0]) {
        show_message("No filename set (Ctrl+F to set name)", COL_STATUS);
        return;
    }

    /* Serialise lines into a flat buffer separated by \n */
    static uint8_t save_buf[MAX_LINES * LINE_LEN];
    uint32_t pos = 0;
    for (int i = 0; i < num_lines && pos < sizeof(save_buf)-1; i++) {
        for (int j = 0; j < line_len[i] && pos < sizeof(save_buf)-1; j++)
            save_buf[pos++] = (uint8_t)lines[i][j];
        if (pos < sizeof(save_buf)-1)
            save_buf[pos++] = '\n';
    }
    save_buf[pos] = '\0';

    int result = myfs_write(filename, save_buf, pos);
    if (result >= 0) {
        modified = 0;
        show_message("Saved.", VGA_COLOR(VGA_BLACK, VGA_GREEN));
    } else {
        show_message("Save failed! (disk full?)", VGA_COLOR(VGA_WHITE, VGA_RED));
    }
}

static void load_file(const char* name) {
    static uint8_t load_buf[MAX_LINES * LINE_LEN];
    int bytes = myfs_read(name, load_buf, sizeof(load_buf)-1);
    if (bytes < 0) return;   /* file not found — start blank */
    load_buf[bytes] = '\0';

    /* Parse flat buffer back into lines */
    num_lines = 0;
    int col   = 0;
    for (int i = 0; i < bytes && num_lines < MAX_LINES; i++) {
        char c = (char)load_buf[i];
        if (c == '\n' || col >= ED_COLS-1) {
            lines[num_lines][col] = '\0';
            line_len[num_lines]   = col;
            num_lines++;
            col = 0;
        } else {
            lines[num_lines][col++] = c;
        }
    }
    /* Last line if file doesn't end with \n */
    if (col > 0 && num_lines < MAX_LINES) {
        lines[num_lines][col] = '\0';
        line_len[num_lines]   = col;
        num_lines++;
    }
    if (num_lines == 0) num_lines = 1;
}

/* ── Entry point ──────────────────────────────────────── */

void editor_open(const char* name) {
    /* Initialise state */
    for (int i = 0; i < MAX_LINES; i++) {
        memset(lines[i], 0, LINE_LEN);
        line_len[i] = 0;
    }
    num_lines = 1;
    cx = cy = view_top = modified = 0;
    memset(filename, 0, sizeof(filename));

    if (name) {
        strncpy(filename, name, MYFS_MAX_NAME - 1);
        load_file(name);
    }

    terminal_clear();
    redraw();

    /* ── Main input loop ──────────────────────────────── */
    while (1) {
        char c = keyboard_getchar();

        /* Control characters */
        if (c == 0x11) break;                  /* Ctrl+Q: quit */

        if (c == 0x13) {                        /* Ctrl+S: save */
            save_file();
            redraw();
            continue;
        }

        /* Arrow keys come as escape sequences: ESC [ A/B/C/D */
        if (c == 0x1B) {
            char c2 = keyboard_getchar();
            if (c2 == '[') {
                char c3 = keyboard_getchar();
                switch (c3) {
                    case 'A':   /* Up */
                        if (cy > 0) cy--;
                        clamp_cx();
                        break;
                    case 'B':   /* Down */
                        if (cy < num_lines-1) cy++;
                        clamp_cx();
                        break;
                    case 'C':   /* Right */
                        if (cx < line_len[cy]) cx++;
                        else if (cy < num_lines-1) { cy++; cx = 0; }
                        break;
                    case 'D':   /* Left */
                        if (cx > 0) cx--;
                        else if (cy > 0) { cy--; cx = line_len[cy]; }
                        break;
                    case 'H':   /* Home */
                        cx = 0;
                        break;
                    case 'F':   /* End */
                        cx = line_len[cy];
                        break;
                    case '5':   /* Page Up (followed by ~) */
                        keyboard_getchar();   /* consume ~ */
                        cy -= ED_ROWS;
                        if (cy < 0) cy = 0;
                        clamp_cx();
                        break;
                    case '6':   /* Page Down */
                        keyboard_getchar();
                        cy += ED_ROWS;
                        if (cy >= num_lines) cy = num_lines-1;
                        clamp_cx();
                        break;
                }
            }
            scroll_to_cursor();
            redraw();
            continue;
        }

        /* Printable + editing */
        switch (c) {
            case '\n': insert_newline(); break;
            case '\b': delete_char();    break;
            default:
                if (c >= 32 && c < 127)
                    insert_char(c);
                break;
        }

        scroll_to_cursor();
        redraw();
    }

    /* Restore normal terminal */
    terminal_clear();
    terminal_setcolor(COL_TEXT);
}
