#include <stdint.h>
#include <stddef.h>
#include "../../include/process.h"
#include "../../include/paging.h"
#include "../../include/heap.h"
#include "../../include/string.h"

#define MAX_PROCESSES 16

static process_t processes[MAX_PROCESSES];
static int next_pid = 1;

void process_init(void) {
    // Initialize process table
    memset(processes, 0, sizeof(processes));
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i].pid = 0;
    }
}

process_t* process_create(void* entry_point) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == 0) {
            process_t* proc = &processes[i];
            memset(proc, 0, sizeof(process_t));

            proc->pid = next_pid++;
            proc->page_dir = paging_create_page_dir();
            if (!proc->page_dir) {
                // Page directory creation failed
                proc->pid = 0; // Mark as free
                return NULL;
            }

            proc->regs.eip = (uint32_t)entry_point;
            proc->regs.esp = 0xBFFFF000;
            proc->regs.ebp = 0xBFFFF000;
            proc->regs.cr3 = (uint32_t)proc->page_dir;
            proc->regs.eflags = 0x202; // Interrupts enabled
            
            /* Initialize all registers to 0 */
            proc->regs.eax = 0;
            proc->regs.ecx = 0;
            proc->regs.edx = 0;
            proc->regs.ebx = 0;
            proc->regs.esi = 0;
            proc->regs.edi = 0;

            /* 
             * Setup Kernel Stack for Context Switching.
             * When the scheduler switches TO this process, it pops registers like an ISR.
             * We need to fake an interrupt stack frame at the top of the KERNEL stack.
             * (Assuming we add a kstack field or use a fixed area for now).
             * For this phase, we'll store the "saved esp" as a pointer to a forged frame.
             */
            
            /* Allocate a kernel stack for this process */
            uint32_t* kstack = (uint32_t*)kmalloc(4096);
            if (!kstack) {
                // Memory allocation failed
                proc->pid = 0; // Mark as free
                return NULL;
            }
            
            /* Set up a simple stack frame for context switching */
            proc->regs.esp = (uint32_t)(kstack + 1024); // Top of stack
            proc->kernel_stack = (uint32_t)kstack; // Save for cleanup

            proc->state = PROC_STATE_READY;
            return proc;
        }
    }
    return NULL;
}

void process_start(process_t* proc) {
    /* Switch to process page directory */
    __asm__ volatile ("mov %0, %%cr3" : : "r"(proc->regs.cr3));

    /* Set up segment registers */
    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        : : : "ax"
    );

    /* Create a fake interrupt stack frame for iret */
    __asm__ volatile (
        "mov %0, %%esp\n"          // Switch to process kernel stack
        "pushl $0x10\n"            // SS
        "pushl %1\n"               // ESP
        "pushl %2\n"               // EFLAGS
        "pushl $0x08\n"            // CS
        "pushl %3\n"               // EIP
        "xor %%eax, %%eax\n"       // Clear registers
        "xor %%ebx, %%ebx\n"
        "xor %%ecx, %%ecx\n"
        "xor %%edx, %%edx\n"
        "xor %%esi, %%esi\n"
        "xor %%edi, %%edi\n"
        "xor %%ebp, %%ebp\n"
        "iret\n"                   // Return to user mode
        : : "r"(proc->regs.esp), "r"(proc->regs.esp), "r"(proc->regs.eflags), "r"(proc->regs.eip)
    );
    __builtin_unreachable();
}