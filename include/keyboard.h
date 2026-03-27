#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/* Called once during kernel init */
void keyboard_init(void);

/*
 * Read one character from the keyboard input queue (blocking).
 * Returns the ASCII character. Special keys:
 *   \n  = Enter
 *   \b  = Backspace
 *   \t  = Tab
 *   0x1B = Escape
 *   Special arrow / function keys are returned as 0 (ignored for now)
 */
char keyboard_getchar(void);

/* Non-blocking: returns 0 if nothing in the queue */
char keyboard_poll(void);

/* Used internally by the IRQ handler */
void keyboard_handler(void);

#endif
