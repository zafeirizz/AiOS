#include "../../include/event.h"
#include "../../include/string.h"

#define EVT_QUEUE_SIZE 64
#define EVT_QUEUE_MASK (EVT_QUEUE_SIZE-1)

static event_t   queue[EVT_QUEUE_SIZE];
static volatile uint8_t q_head = 0, q_tail = 0;

void event_push(const event_t* e) {
    uint8_t next = (q_tail + 1) & EVT_QUEUE_MASK;
    if (next == q_head) return;   /* drop if full */
    queue[q_tail] = *e;
    q_tail = next;
}

int event_poll(event_t* out) {
    if (q_head == q_tail) return 0;
    *out = queue[q_head];
    q_head = (q_head + 1) & EVT_QUEUE_MASK;
    return 1;
}

void event_wait(event_t* out) {
    while (!event_poll(out))
        __asm__ volatile("hlt");
}
