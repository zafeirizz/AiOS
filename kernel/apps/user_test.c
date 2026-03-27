#include <stdint.h>

/**
 * Simple kernel mode test process
 * Demonstrates multitasking in kernel mode
 */

void user_main(void) {
    /* Print directly to VGA (kernel mode) */
    extern void terminal_write(const char* str);
    
    terminal_write("Hello from kernel process!\n");
    
    /* Loop forever to demonstrate multitasking */
    int counter = 0;
    while (1) {
        counter++;
        if (counter % 1000000 == 0) {
            terminal_write("Kernel process tick\n");
        }
    }
}