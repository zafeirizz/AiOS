#include <stdint.h>
#include "../../include/mouse.h"
#include "../../include/irq.h"
#include "../../include/fb.h"
#include "../../include/event.h"

static inline uint8_t inb(uint16_t p) {
    uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v;
}
static inline void outb(uint16_t p, uint8_t v) {
    __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));
}

#define PS2_DATA  0x60
#define PS2_CMD   0x64

static void ps2_wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(PS2_CMD) & 2)) return;
}
static void ps2_wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(PS2_CMD) & 1) return;
}
static void mouse_write(uint8_t byte) {
    ps2_wait_write(); outb(PS2_CMD, 0xD4);
    ps2_wait_write(); outb(PS2_DATA, byte);
}
static uint8_t mouse_read(void) {
    ps2_wait_read(); return inb(PS2_DATA);
}

static mouse_state_t state = {0};
static uint8_t packet[3];
static int     packet_phase = 0;

void mouse_handler(void) {
    uint8_t byte = inb(PS2_DATA);

    if (packet_phase == 0 && !(byte & 0x08))
        return;   /* wait for sync byte */

    packet[packet_phase++] = byte;
    if (packet_phase < 3) return;
    packet_phase = 0;

    int dx =  (int)(int8_t)packet[1];
    int dy = -(int)(int8_t)packet[2];   /* Y is inverted */

    int prev_x = state.x, prev_y = state.y;
    state.dx = dx;
    state.dy = dy;
    state.x += dx;
    state.y += dy;
    if (state.x < 0) state.x = 0;
    if (state.y < 0) state.y = 0;
    if (state.x >= FB_WIDTH)  state.x = FB_WIDTH  - 1;
    if (state.y >= FB_HEIGHT) state.y = FB_HEIGHT - 1;

    int buttons = packet[0] & 0x07;
    int prev_btn = state.buttons;
    state.buttons = buttons;

    /* Push events */
    event_t e;
    if (state.x != prev_x || state.y != prev_y) {
        e.type = EVT_MOUSE_MOVE;
        e.mouse.x = state.x; e.mouse.y = state.y;
        e.mouse.dx = dx; e.mouse.dy = dy; e.mouse.button = 0;
        event_push(&e);
    }
    for (int b = 0; b < 3; b++) {
        int mask = 1 << b;
        if ((buttons & mask) && !(prev_btn & mask)) {
            e.type = EVT_MOUSE_DOWN;
            e.mouse.x = state.x; e.mouse.y = state.y;
            e.mouse.button = b;
            event_push(&e);
        } else if (!(buttons & mask) && (prev_btn & mask)) {
            e.type = EVT_MOUSE_UP;
            e.mouse.x = state.x; e.mouse.y = state.y;
            e.mouse.button = b;
            event_push(&e);
        }
    }
}

void mouse_init(void) {
    /* Enable auxiliary device */
    ps2_wait_write(); outb(PS2_CMD, 0xA8);
    /* Enable interrupt for mouse (IRQ12) */
    ps2_wait_write(); outb(PS2_CMD, 0x20);
    ps2_wait_read();
    uint8_t status = inb(PS2_DATA) | 2;
    ps2_wait_write(); outb(PS2_CMD, 0x60);
    ps2_wait_write(); outb(PS2_DATA, status);
    /* Set defaults, enable */
    mouse_write(0xF6); mouse_read();
    mouse_write(0xF4); mouse_read();

    irq_register_handler(12, mouse_handler);
    irq_clear_mask(12);
    /* Cascade IRQ2 must also be unmasked for slave PIC */
    irq_clear_mask(2);
}

const mouse_state_t* mouse_get(void) { return &state; }
