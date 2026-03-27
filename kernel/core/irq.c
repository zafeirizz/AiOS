#include <stdint.h>
#include "../../include/irq.h"
#include "../../include/idt.h"
#include "../../include/scheduler.h"

/* ─── I/O helpers ────────────────────────────────────── */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ─── PIC constants ──────────────────────────────────── */

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

/* ─── IRQ stub declarations (from isr.asm) ───────────── */

extern void irq0(void);  extern void irq1(void);
extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);
extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void);
extern void irq14(void); extern void irq15(void);

/* ─── Handler table ──────────────────────────────────── */

static irq_handler_t irq_handlers[16] = { 0 };

/* ─── PIC functions ──────────────────────────────────── */

void pic_remap(void) {
    /* ICW1: start init sequence */
    outb(PIC1_CMD,  0x11);
    outb(PIC2_CMD,  0x11);

    /* ICW2: vector offsets — IRQ0-7 → int 32-39, IRQ8-15 → int 40-47 */
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    /* ICW3: cascade wiring */
    outb(PIC1_DATA, 0x04);   /* master: slave on IRQ2 */
    outb(PIC2_DATA, 0x02);   /* slave:  cascade identity = 2 */

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    /* Mask everything — drivers unmask their own line via irq_clear_mask().
       Never restore the old BIOS masks: they are 0xFF at this point. */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void irq_set_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (irq < 8) ? irq : (irq - 8);
    outb(port, inb(port) | (1 << bit));
}

void irq_clear_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (irq < 8) ? irq : (irq - 8);
    outb(port, inb(port) & ~(1 << bit));
}

/* ─── Register / dispatch ────────────────────────────── */

void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16)
        irq_handlers[irq] = handler;
}

/*
 * Called from irq_common_stub in isr.asm.
 * Dispatches to the registered handler and sends EOI.
 */
void irq_dispatch(uint8_t irq, void* esp) {
    if (irq < 16 && irq_handlers[irq])
        irq_handlers[irq]();

    /* Timer interrupt (IRQ 0) triggers scheduler */
    if (irq == 0) {
        scheduler_tick(esp);
    }

    pic_send_eoi(irq);
}

/* ─── Init: remap PIC, wire IRQ stubs into IDT ───────── */

void irq_init(void) {
    /* Start with all IRQs masked — drivers unmask their own */
    pic_remap();
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    /* Wire IRQ stubs into IDT vectors 32-47 */
    idt_set_gate(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);
}
