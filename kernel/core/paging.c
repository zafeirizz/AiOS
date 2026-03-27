#include <stdint.h>
#include <stddef.h>
#include "../../include/paging.h"
#include "../../include/heap.h"

/*
 * We need to identity-map:
 *   0x00000000 - 0x00FFFFFF  (16 MB) — kernel + BSS (fb_back is ~3MB) + heap
 *   0xE0000000 - 0xE0FFFFFF  (16 MB) — typical QEMU VBE framebuffer region
 *
 * Page directory entry N covers virtual 4MB region starting at N*4MB.
 * We use one page table per 4MB region we want to map.
 */

/* 4 page tables for low 16 MB (entries 0-3) */
static uint32_t pt_low[4][1024] __attribute__((aligned(4096)));

/* 4 page tables for framebuffer region 0xE0000000-0xE0FFFFFF (entries 896-899) */
static uint32_t pt_fb[4][1024]  __attribute__((aligned(4096)));

static uint32_t page_directory[1024] __attribute__((aligned(4096)));

extern uint32_t fb_phys_addr;

void paging_init(void) {
    int i, j;

    /* Clear page directory */
    for (i = 0; i < 1024; i++)
        page_directory[i] = 0;

    /* Identity-map low 16 MB (4 page tables × 4 MB = 16 MB) */
    for (j = 0; j < 4; j++) {
        uint32_t base = (uint32_t)j * 1024 * 0x1000;
        for (i = 0; i < 1024; i++)
            pt_low[j][i] = (base + (uint32_t)i * 0x1000) | 0x7; /* 0x7 = User | RW | Present */
        page_directory[j] = (uint32_t)pt_low[j] | 0x7;
    }

    /*
     * Map framebuffer region dynamically based on Multiboot info.
     */
    if (fb_phys_addr != 0) {
        uint32_t fb_dir_idx = fb_phys_addr >> 22;
        for (j = 0; j < 4; j++) {
            uint32_t base = (fb_dir_idx + j) << 22; // Align to 4MB chunk
            for (i = 0; i < 1024; i++)
                pt_fb[j][i] = (base + (uint32_t)i * 0x1000) | 0x3;
            page_directory[fb_dir_idx + j] = (uint32_t)pt_fb[j] | 0x3;
        }
    }

    /* Load CR3 */
    __asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory) : "memory");

    /* Enable paging (bit 31 of CR0) */
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

uint32_t* paging_create_page_dir(void) {
    uint32_t* new_pd = (uint32_t*)kmalloc(4096);
    if (!new_pd) return NULL;

    // Copy kernel mappings
    for (int i = 0; i < 1024; i++) {
        new_pd[i] = page_directory[i];
    }

    // User space will be mapped separately when loading ELF or allocating memory

    return new_pd;
}
