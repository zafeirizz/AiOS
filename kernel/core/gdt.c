#include <stdint.h>
#include "../../include/gdt.h"
#include "../../include/string.h"

/* Defines a GDT entry */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* Defines the TSS structure */
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

/* GDT with 6 entries: Null, KCode, KData, UCode, UData, TSS */
static struct gdt_entry gdt[6];
static struct gdt_ptr   gp;
static struct tss_entry tss_entry;

/* Dedicated kernel stack for syscalls/interrupts */
static uint8_t sys_stack[16384]; // 16 KB kernel stack

/* Setup a descriptor in the Global Descriptor Table */
static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);

    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access      = access;
}

/* Initialize the TSS */
static void write_tss(int num, uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t) &tss_entry;
    uint32_t limit = sizeof(tss_entry);

    /* Access 0x89 = Present | Ring0 | System | 32-bit TSS Available */
    gdt_set_gate(num, base, limit, 0x89, 0x00);

    memset(&tss_entry, 0, sizeof(tss_entry));

    tss_entry.ss0  = ss0;
    tss_entry.esp0 = esp0;
    
    // No IOPB
    tss_entry.iomap_base = sizeof(tss_entry);
}

void gdt_init(void) {
    gp.limit = (sizeof(struct gdt_entry) * 6) - 1;
    gp.base  = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                // 0: Null
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // 1: KCode (0x08)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // 2: KData (0x10)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // 3: UCode (0x18)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // 4: UData (0x20)
    
    // 5: TSS (0x28) - Set initial kernel stack
    write_tss(5, 0x10, (uint32_t)sys_stack + sizeof(sys_stack));

    // Flush GDT and Load TSS
    __asm__ volatile (
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds; mov %%ax, %%es; mov %%ax, %%fs; mov %%ax, %%gs; mov %%ax, %%ss\n"
        "ljmp $0x08, $.flush\n"
        ".flush:\n"
        "ltr %%bx\n"
        : : "m"(gp), "b"((uint16_t)0x28) : "eax"
    );
}

void tss_set_stack(uint32_t esp0) {
    tss_entry.esp0 = esp0;
}