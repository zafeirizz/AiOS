#ifndef IDT_H
#define IDT_H
#include <stdint.h>

#define KERNEL_CODE_SEG 0x08
#define KERNEL_DATA_SEG 0x10
#define USER_CODE_SEG   0x18
#define USER_DATA_SEG   0x20

void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags);
#endif
