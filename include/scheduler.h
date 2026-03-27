#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

void scheduler_init(void);
void scheduler_start(void);  /* Start multitasking */
void scheduler_add_process(process_t* proc);
void scheduler_remove_process(int pid);
int scheduler_process_count(void);
process_t* scheduler_get_current(void);

/* Called by ISR to switch stacks */
void scheduler_tick(void* esp);

#endif