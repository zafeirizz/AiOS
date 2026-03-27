#include <stdint.h>
#include "../../include/syscall.h"
#include "../../include/vga.h"

void syscall_handler(void) {
    uint32_t syscall_num;
    uint32_t arg1, arg2, arg3;

    // Get syscall number and args from registers
    __asm__ volatile (
        "mov %%eax, %0\n"
        "mov %%ebx, %1\n"
        "mov %%ecx, %2\n"
        "mov %%edx, %3\n"
        : "=r"(syscall_num), "=r"(arg1), "=r"(arg2), "=r"(arg3)
    );

    switch (syscall_num) {
        case 0:  // write
            // For now, just print to VGA
            terminal_write((const char*)arg1);
            break;
        case 1:  // read
            // TODO
            break;
        case 2:  // open
            // TODO
            break;
        case 3:  // exec
            // TODO
            break;
        case 4:  // exit
            terminal_write("Syscall: exit/yield called. Halting.\n");
            while(1) __asm__("hlt");
            break;
        default:
            terminal_write("Unknown syscall\n");
            break;
    }
}