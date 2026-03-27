#include <stdint.h>
#include "../../include/vga.h"
#include "../../include/isr.h"

extern void syscall_handler(void);  // Add this

static const char* exception_names[32] = {
    "Divide by zero",           /*  0 */
    "Debug",                    /*  1 */
    "Non-maskable interrupt",   /*  2 */
    "Breakpoint",               /*  3 */
    "Overflow",                 /*  4 */
    "Bound range exceeded",     /*  5 */
    "Invalid opcode",           /*  6 */
    "Device not available",     /*  7 */
    "Double fault",             /*  8 */
    "Coprocessor segment overrun", /* 9 */
    "Invalid TSS",              /* 10 */
    "Segment not present",      /* 11 */
    "Stack-segment fault",      /* 12 */
    "General protection fault", /* 13 */
    "Page fault",               /* 14 */
    "Reserved",                 /* 15 */
    "x87 FPU error",            /* 16 */
    "Alignment check",          /* 17 */
    "Machine check",            /* 18 */
    "SIMD FP exception",        /* 19 */
    "Virtualization exception", /* 20 */
    "Control protection",       /* 21 */
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Security exception",       /* 30 */
    "Reserved",                 /* 31 */
};

void isr_handler(uint32_t int_no, uint32_t err_code) {
    if (int_no == 128) {
        syscall_handler();
        return;
    }

    terminal_setcolor(VGA_COLOR(VGA_WHITE, VGA_RED));
    terminal_write("\n*** KERNEL EXCEPTION ***\n");
    terminal_setcolor(VGA_COLOR(VGA_LIGHT_RED, VGA_BLACK));

    if (int_no < 32)
        terminal_write(exception_names[int_no]);
    else
        terminal_write("Unknown exception");

    terminal_write("  (vector=0x");
    terminal_write_hex(int_no);
    terminal_write("  err=0x");
    terminal_write_hex(err_code);
    terminal_write(")\n");

    /* For page faults, CR2 holds the faulting address */
    if (int_no == 14) {
        uint32_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        terminal_write("Faulting address: 0x");
        terminal_write_hex(cr2);
        terminal_write("\n");
    }

    terminal_setcolor(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    terminal_write("System halted.\n");

    __asm__ volatile ("cli");
    while (1) __asm__ volatile ("hlt");
}
