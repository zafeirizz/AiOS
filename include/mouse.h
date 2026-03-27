#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct {
    int x, y;           /* absolute position (clamped to screen) */
    int dx, dy;         /* delta since last packet               */
    int buttons;        /* bitmask: bit0=left, bit1=right, bit2=middle */
} mouse_state_t;

void             mouse_init(void);
const mouse_state_t* mouse_get(void);
void             mouse_handler(void);   /* called by IRQ12 */

#endif
