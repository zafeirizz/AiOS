#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>

typedef enum {
    EVT_NONE = 0,
    EVT_KEY_DOWN,
    EVT_KEY_UP,
    EVT_MOUSE_MOVE,
    EVT_MOUSE_DOWN,
    EVT_MOUSE_UP,
    EVT_MOUSE_SCROLL,
} event_type_t;

typedef struct {
    event_type_t type;
    union {
        struct { char c; int scancode; }          key;
        struct { int x, y, dx, dy, button; }      mouse;
    };
} event_t;

/* Push an event onto the queue (called from IRQ handlers) */
void event_push(const event_t* e);

/* Pop next event. Returns 1 if got one, 0 if queue empty. */
int  event_poll(event_t* out);

/* Block until an event is available */
void event_wait(event_t* out);

#endif
