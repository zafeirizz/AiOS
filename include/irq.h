#ifndef IRQ_H
#define IRQ_H
#include <stdint.h>
typedef void (*irq_handler_t)(void);
void irq_register_handler(uint8_t irq, irq_handler_t handler);
void irq_dispatch(uint8_t irq, void* esp);
void irq_set_mask(uint8_t irq);
void irq_clear_mask(uint8_t irq);
void pic_remap(void);
void pic_send_eoi(uint8_t irq);
void irq_init(void);
#endif
