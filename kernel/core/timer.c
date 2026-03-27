#include <stdint.h>
#include "../../include/timer.h"
#include "../../include/irq.h"
#include "../../include/scheduler.h"

/* ─── I/O helper ─────────────────────────────────────── */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ─── PIT constants ──────────────────────────────────── */

#define PIT_CMD      0x43
#define PIT_CHANNEL0 0x40
#define PIT_BASE_HZ  1193180

/* ─── State ──────────────────────────────────────────── */

static volatile uint32_t ticks     = 0;
static          uint32_t tick_rate = 100;   /* Hz, set by timer_init */

/* ─── IRQ0 handler ───────────────────────────────────── */

static void timer_irq_handler(void) {
    ticks++;
}

/* ─── Public API ─────────────────────────────────────── */

void timer_init(uint32_t frequency_hz) {
    tick_rate = frequency_hz;

    uint32_t divisor = PIT_BASE_HZ / frequency_hz;

    /* Channel 0, lobyte/hibyte, rate generator (mode 2) */
    outb(PIT_CMD,      0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    /* Register handler and unmask IRQ0 */
    irq_register_handler(0, timer_irq_handler);
    irq_clear_mask(0);

    /* Initialize scheduler for multitasking */
    scheduler_init();
}

uint32_t timer_get_ticks(void) {
    return ticks;
}

void timer_sleep_ms(uint32_t ms) {
    /* ticks per ms = tick_rate / 1000 */
    uint32_t end = ticks + (ms * tick_rate) / 1000;
    while (ticks < end)
        __asm__ volatile ("hlt");   /* sleep until next IRQ */
}
