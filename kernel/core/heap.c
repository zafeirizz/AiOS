#include <stdint.h>
#include "../../include/heap.h"

static uint32_t heap_base    = 0;
static uint32_t heap_current = 0;
static uint32_t heap_limit   = 0;

void heap_init(uint32_t start, uint32_t size) {
    heap_base    = start;
    heap_current = start;
    heap_limit   = start + size;
}

void* kmalloc(uint32_t size) {
    /* Align up to 4-byte boundary before returning */
    if (heap_current & 0x3)
        heap_current = (heap_current & ~0x3) + 4;

    if (heap_current + size > heap_limit)
        return (void*)0;    /* out of memory */

    void* ptr     = (void*)heap_current;
    heap_current += size;
    return ptr;
}

/*
 * kfree is a stub — the bump allocator cannot reclaim memory.
 * Phase 3 will replace this with a proper free-list allocator.
 */
void kfree(void* ptr) {
    (void)ptr;
}

uint32_t heap_used(void) {
    return heap_current - heap_base;
}
