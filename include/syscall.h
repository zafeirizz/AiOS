#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* Syscall numbers */
#define SYS_WRITE 0
#define SYS_READ  1
#define SYS_OPEN  2
#define SYS_EXEC  3
#define SYS_EXIT  4

/* Syscall function */
static inline void syscall(uint32_t number, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    __asm__ volatile (
        "int $0x80"
        : : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3)
    );
}

void syscall_handler(void);

#endif