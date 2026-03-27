#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

typedef enum {
    PROC_STATE_FREE,
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_BLOCKED,
} proc_state_t;

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp;
    uint32_t eip, esp, eflags;
    uint32_t cr3;
} proc_regs_t;

typedef struct process_s {
    int pid;
    proc_state_t state;
    proc_regs_t regs;
    uint32_t* page_dir;
    uint32_t kernel_stack;  /* Base address of kernel stack for cleanup */
} process_t;

void process_init(void);
process_t* process_create(void* entry_point);
void process_start(process_t* proc);

#endif