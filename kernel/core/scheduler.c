#include <stdint.h>
#include <stddef.h>
#include "../../include/scheduler.h"
#include "../../include/paging.h"
#include "../../include/string.h"
#include "../../include/gdt.h" // For tss_set_stack

/**
 * SCHEDULER IMPLEMENTATION
 * 
 * Round-robin scheduling with context switching on timer tick.
 * Processes maintain their own ESP/EBP/EIP and page directory.
 */

#define READY_QUEUE_SIZE 64

typedef struct {
    process_t* processes[READY_QUEUE_SIZE];
    int count;
    int current_idx;
} ready_queue_t;

static ready_queue_t ready_queue = { 0 };
static process_t* current_process = NULL;
static int scheduling_enabled = 0;

/* ─────────────────────────────────────────────────────────────
   Initialization
   ───────────────────────────────────────────────────────────── */

void scheduler_init(void) {
    memset(&ready_queue, 0, sizeof(ready_queue_t));
    current_process = NULL;
    scheduling_enabled = 0;
}

void scheduler_start(void) {
    if (ready_queue.count > 0) {
        current_process = ready_queue.processes[0];
        current_process->state = PROC_STATE_RUNNING;
        scheduling_enabled = 1;
        
        /* Start the first process */
        extern void process_start(process_t* proc);
        process_start(current_process);
        /* process_start never returns */
    }
}

/* ─────────────────────────────────────────────────────────────
   Queue Management
   ───────────────────────────────────────────────────────────── */

void scheduler_add_process(process_t* proc) {
    if (!proc) return;
    if (ready_queue.count >= READY_QUEUE_SIZE) return;
    
    ready_queue.processes[ready_queue.count++] = proc;
    proc->state = PROC_STATE_READY;
    
    /* If this is the first process, make it current */
    if (ready_queue.count == 1) {
        current_process = proc;
        proc->state = PROC_STATE_RUNNING;
        ready_queue.current_idx = 0;
    }
}

void scheduler_remove_process(int pid) {
    for (int i = 0; i < ready_queue.count; i++) {
        if (ready_queue.processes[i]->pid == pid) {
            /* Remove from queue */
            for (int j = i; j < ready_queue.count - 1; j++) {
                ready_queue.processes[j] = ready_queue.processes[j + 1];
            }
            ready_queue.count--;
            
            /* Adjust current index if needed */
            if (ready_queue.current_idx >= ready_queue.count && ready_queue.count > 0) {
                ready_queue.current_idx = 0;
            }
            
            return;
        }
    }
}

int scheduler_process_count(void) {
    return ready_queue.count;
}

process_t* scheduler_get_current(void) {
    return current_process;
}

/* ─────────────────────────────────────────────────────────────
   Context Switching (Called from timer interrupt)
   
   Called with ESP pointing to saved register frame on stack:
   [esp+0]   : eax (from pushad)
   [esp+4]   : ecx
   [esp+8]   : edx
   [esp+12]  : ebx
   [esp+16]  : esp_dummy (ignored)
   [esp+20]  : ebp
   [esp+24]  : esi
   [esp+28]  : edi
   [esp+32]  : ds
   [esp+36]  : es
   [esp+40]  : fs
   [esp+44]  : gs
   [esp+48]  : eip (or will be after more stack unwinding in irq_common_stub)
   
   We save the current process's ESP, select next, and modify the 
   stack frame to contain the next process's register state.
   ───────────────────────────────────────────────────────────── */

void scheduler_tick(void* esp) {
    if (!scheduling_enabled || ready_queue.count <= 1) return;
    
    /* Save current process state if running */
    if (current_process && current_process->state == PROC_STATE_RUNNING) {
        /* Save all registers from the stack frame */
        uint32_t* frame = (uint32_t*)esp;
        current_process->regs.eax = frame[0];
        current_process->regs.ecx = frame[1];
        current_process->regs.edx = frame[2];
        current_process->regs.ebx = frame[3];
        current_process->regs.ebp = frame[5];
        current_process->regs.esi = frame[6];
        current_process->regs.edi = frame[7];
        current_process->regs.esp = (uint32_t)esp;
        current_process->state = PROC_STATE_READY;
    }
    
    /* Round-robin: move to next process */
    ready_queue.current_idx = (ready_queue.current_idx + 1) % ready_queue.count;
    current_process = ready_queue.processes[ready_queue.current_idx];
    current_process->state = PROC_STATE_RUNNING;
    
    /* Restore next process's page directory */
    __asm__ volatile ("mov %0, %%cr3" : : "r"(current_process->regs.cr3));
    
    /* Restore next process's registers into the stack frame */
    uint32_t* frame = (uint32_t*)esp;
    frame[0] = current_process->regs.eax;
    frame[1] = current_process->regs.ecx;
    frame[2] = current_process->regs.edx;
    frame[3] = current_process->regs.ebx;
    frame[5] = current_process->regs.ebp;
    frame[6] = current_process->regs.esi;
    frame[7] = current_process->regs.edi;
}

/* ─────────────────────────────────────────────────────────────
   Debug/Diagnostics
   ───────────────────────────────────────────────────────────── */

void scheduler_dump_state(void) {
    extern void terminal_write(const char*);
    extern void terminal_write_dec(uint32_t);
    
    terminal_write("\n=== SCHEDULER STATE ===\n");
    terminal_write("Processes in queue: ");
    terminal_write_dec(ready_queue.count);
    terminal_write("\n");
    
    if (current_process) {
        terminal_write("Current PID: ");
        terminal_write_dec(current_process->pid);
        terminal_write("\n");
    }
    
    for (int i = 0; i < ready_queue.count; i++) {
        process_t* p = ready_queue.processes[i];
        terminal_write("  [");
        terminal_write_dec(i);
        terminal_write("] PID=");
        terminal_write_dec(p->pid);
        terminal_write(" State=");
        terminal_write_dec(p->state);
        terminal_write("\n");
    }
}
