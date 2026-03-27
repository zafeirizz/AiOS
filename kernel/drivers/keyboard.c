#include <stdint.h>
#include "../../include/keyboard.h"
#include "../../include/irq.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t r; __asm__ volatile ("inb %1,%0":"=a"(r):"Nd"(port)); return r;
}

/* ── Circular input queue ─────────────────────────────── */

#define QSIZE 64
#define QMASK (QSIZE-1)
static volatile char     queue[QSIZE];
static volatile uint8_t  q_head = 0;
static volatile uint8_t  q_tail = 0;

static void qpush(char c) {
    uint8_t next = (q_tail+1) & QMASK;
    if (next != q_head) { queue[q_tail] = c; q_tail = next; }
}
static int qempty(void)  { return q_head == q_tail; }
static char qpop(void)   { char c = queue[q_head]; q_head=(q_head+1)&QMASK; return c; }

/* ── US QWERTY scancode tables ────────────────────────── */

static const char sc_normal[128] = {
/*00*/  0,    0,   '1', '2', '3', '4', '5', '6',
/*08*/ '7',  '8', '9', '0', '-', '=', '\b', '\t',
/*10*/ 'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',
/*18*/ 'o',  'p', '[', ']', '\n', 0,  'a', 's',
/*20*/ 'd',  'f', 'g', 'h', 'j', 'k', 'l', ';',
/*28*/ '\'', '`',  0,  '\\','z', 'x', 'c', 'v',
/*30*/ 'b',  'n', 'm', ',', '.', '/',  0,   0,
/*38*/  0,   ' ',  0,   0,   0,   0,   0,   0,
/*40*/  0,    0,   0,   0,   0,   0,   0,   0,
/*48*/  0,    0,   0,   0,   0,   0,   0,   0,
/*50*/  0,    0,   0,   0,   0,   0,   0,   0,
/*58*/  0,    0,   0,   0,   0,   0,   0,   0,
};

static const char sc_shifted[128] = {
/*00*/  0,    0,   '!', '@', '#', '$', '%', '^',
/*08*/ '&',  '*', '(', ')', '_', '+', '\b', '\t',
/*10*/ 'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I',
/*18*/ 'O',  'P', '{', '}', '\n', 0,  'A', 'S',
/*20*/ 'D',  'F', 'G', 'H', 'J', 'K', 'L', ':',
/*28*/ '"',  '~',  0,  '|', 'Z', 'X', 'C', 'V',
/*30*/ 'B',  'N', 'M', '<', '>', '?',  0,   0,
/*38*/  0,   ' ',  0,   0,   0,   0,   0,   0,
/*40*/  0,    0,   0,   0,   0,   0,   0,   0,
/*48*/  0,    0,   0,   0,   0,   0,   0,   0,
/*50*/  0,    0,   0,   0,   0,   0,   0,   0,
/*58*/  0,    0,   0,   0,   0,   0,   0,   0,
};

/* ── Modifier state ───────────────────────────────────── */

static int shift    = 0;
static int ctrl     = 0;
static int capslock = 0;
static int extended = 0;   /* 1 after receiving 0xE0 prefix */

/* ── IRQ1 handler ─────────────────────────────────────── */

static void keyboard_irq(void) {
    uint8_t sc = inb(0x60);

    /* 0xE0 prefix: next scancode is an extended key */
    if (sc == 0xE0) { extended = 1; return; }

    int ext = extended;
    extended = 0;

    /* ── Key release ──────────────────────────────────── */
    if (sc & 0x80) {
        uint8_t key = sc & 0x7F;
        if (!ext) {
            if (key == 0x2A || key == 0x36) shift = 0;
            if (key == 0x1D)                ctrl  = 0;
        } else {
            if (key == 0x1D) ctrl = 0;   /* right Ctrl release */
        }
        return;
    }

    /* ── Extended (E0-prefixed) keys ──────────────────── */
    if (ext) {
        switch (sc) {
            case 0x48: qpush(0x1B);qpush('[');qpush('A'); return; /* Up    */
            case 0x50: qpush(0x1B);qpush('[');qpush('B'); return; /* Down  */
            case 0x4D: qpush(0x1B);qpush('[');qpush('C'); return; /* Right */
            case 0x4B: qpush(0x1B);qpush('[');qpush('D'); return; /* Left  */
            case 0x47: qpush(0x1B);qpush('[');qpush('H'); return; /* Home  */
            case 0x4F: qpush(0x1B);qpush('[');qpush('F'); return; /* End   */
            case 0x49: qpush(0x1B);qpush('[');qpush('5');qpush('~'); return; /* PgUp */
            case 0x51: qpush(0x1B);qpush('[');qpush('6');qpush('~'); return; /* PgDn */
            case 0x1D: ctrl = 1; return;   /* right Ctrl press */
            default:   return;
        }
    }

    /* ── Normal keys ──────────────────────────────────── */
    if (sc == 0x2A || sc == 0x36) { shift = 1;        return; }
    if (sc == 0x1D)               { ctrl  = 1;        return; }
    if (sc == 0x3A)               { capslock ^= 1;    return; }
    if (sc == 0x01)               { qpush(0x1B);      return; } /* Escape */

    if (sc >= 128) return;

    /* Ctrl+letter → control character (Ctrl+Q=0x11, Ctrl+S=0x13, etc.) */
    if (ctrl) {
        char base = sc_normal[sc];
        if (base >= 'a' && base <= 'z') { qpush(base - 'a' + 1); return; }
        if (base >= 'A' && base <= 'Z') { qpush(base - 'A' + 1); return; }
        return;
    }

    char c = shift ? sc_shifted[sc] : sc_normal[sc];
    if (capslock && c >= 'a' && c <= 'z') c -= 32;
    if (capslock && c >= 'A' && c <= 'Z') c += 32;
    if (c) qpush(c);
}

/* ── Public API ───────────────────────────────────────── */

void keyboard_init(void) {
    irq_register_handler(1, keyboard_irq);
    irq_clear_mask(1);
}

char keyboard_getchar(void) {
    while (qempty()) __asm__ volatile ("hlt");
    return qpop();
}

char keyboard_poll(void) {
    if (qempty()) return 0;
    return qpop();
}

void keyboard_handler(void) { keyboard_irq(); }
