#include <stdint.h>
#include <stddef.h>
#include "../include/vga.h"
#include "../include/idt.h"
#include "../include/gdt.h"
#include "../include/irq.h"
#include "../include/isr.h"
#include "../include/timer.h"
#include "../include/keyboard.h"
#include "../include/mouse.h"
#include "../include/paging.h"
#include "../include/heap.h"
#include "../include/shell.h"
#include "../include/ata.h"
#include "../include/e1000.h"
#include "../include/ethernet.h"
#include "../include/arp.h"
#include "../include/ip.h"
#include "../include/tcp.h"
#include "../include/net.h"
#include "../include/fb.h"
#include "../include/fat32.h"
#include "../include/zfsx.h"
#include "../include/gfx.h"
#include "../include/wm.h"
#include "../include/fileman.h"
#include "../include/settings.h"
#include "../include/diskmgr.h"
#include "../include/scheduler.h"

#include "../include/process.h"
#include "../include/user_test.h"
#define HEAP_START 0x00400000
#define HEAP_SIZE  0x00C00000   /* 12 MB — GUI back buffer needs ~3 MB */

#define DEFAULT_IP      IP4(10,0,2,15)
#define DEFAULT_GW      IP4(10,0,2,2)
#define DEFAULT_NETMASK IP4(255,255,255,0)

/* Variables defined in fb_simple.c */
extern uint32_t fb_phys_addr;
extern uint32_t fb_pitch;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_bpp;

/* Multiboot2 header definitions */
struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_tag_framebuffer {
    struct multiboot_tag common;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
};

void kernel_main(uint32_t magic, uint32_t addr) {
    /* Parse Multiboot2 info to get framebuffer address */
    if (magic == 0x36d76289) {
        struct multiboot_tag *tag = (struct multiboot_tag *)(addr + 8);
        while (tag->type != 0) {
            if (tag->type == 8) { /* FRAMEBUFFER */
                struct multiboot_tag_framebuffer *fb = (struct multiboot_tag_framebuffer *)tag;
                fb_phys_addr = (uint32_t)fb->framebuffer_addr;
                fb_pitch     = fb->framebuffer_pitch;
                fb_width     = fb->framebuffer_width;
                fb_height    = fb->framebuffer_height;
                fb_bpp       = fb->framebuffer_bpp;
            }
            /* Next tag (aligned to 8 bytes) */
            tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7));
        }
    }

    /* ── Text mode boot log (before GUI takes over) ────── */
    vga_init();
    terminal_setcolor(VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
    terminal_writeline("=== myOS Phase 6 — GUI ===");
    terminal_setcolor(VGA_COLOR(VGA_WHITE, VGA_BLACK));

    paging_init();
    terminal_writeline("[OK] Paging");

    gdt_init();
    terminal_writeline("[OK] GDT & TSS");

    heap_init(HEAP_START, HEAP_SIZE);
    terminal_writeline("[OK] Heap (8 MB)");

    idt_init();
    irq_init();
    terminal_writeline("[OK] IDT/IRQ");

    timer_init(100);
    terminal_writeline("[OK] Timer");

    scheduler_init();
    terminal_writeline("[OK] Scheduler");

    /* Initialize process system and create test processes */
    process_init();
    terminal_writeline("[OK] Process system");

    /* TEMPORARILY DISABLED: Process creation causing crash
    process_t* proc1 = process_create(user_main);
    if (proc1) {
        scheduler_add_process(proc1);
        terminal_writeline("[OK] Created kernel process 1");
    }

    process_t* proc2 = process_create(user_main);
    if (proc2) {
        scheduler_add_process(proc2);
        terminal_writeline("[OK] Created kernel process 2");
    }

    scheduler_start();
    terminal_writeline("[OK] Multitasking started");
    */

    keyboard_init();
    terminal_writeline("[OK] Keyboard");

    __asm__ volatile ("sti");
    terminal_writeline("[OK] Interrupts");

    /* ATA */
    if (ata_init())
        terminal_writeline("[OK] ATA disk");
    else
        terminal_writeline("[--] No ATA disk");

    /* Filesystems */
    if (fat32_mount_mbr() == 0)
        terminal_writeline("[OK] FAT32 mounted");
    else
        terminal_writeline("[--] FAT32 mount failed");

    if (zfsx_mount(1) == 0)
        terminal_writeline("[OK] ZFSX mounted");
    else
        terminal_writeline("[--] ZFSX mount failed");

    /* Network */
    arp_init();
    ip_init(DEFAULT_IP, DEFAULT_GW, DEFAULT_NETMASK);
    tcp_init();
    
    int nic_found = 0;
    if (rtl8139_init() == 0) {
        ethernet_init();
        terminal_writeline("[OK] Network (RTL8139)");
        nic_found = 1;
    } else if (e1000_init(0) == 0) {  /* Try e1000 at PCI address 0 */
        ethernet_init();
        terminal_writeline("[OK] Network (e1000)");
        nic_found = 1;
    }
    
    if (!nic_found) {
        terminal_writeline("[--] No NIC found");
    }

    /* ── Framebuffer ─────────────────────────────────── */
    fb_init();

    if (fb_addr == (void*)0) {
        /* No framebuffer — fall back to text mode shell */
        terminal_writeline("[!!] No framebuffer — falling back to text shell");
        terminal_writeline("     Add to grub.cfg:  set gfxpayload=1024x768x32");
        extern void shell_init(void);
        shell_init();
        while (1) {
            __asm__ volatile ("hlt");
            char c;
            while ((c = keyboard_poll()) != 0)
                shell_handle_input(c);
            ethernet_poll();
        }
    }

    /* ── User mode test ──────────────────────────────── */
    /* COMMENTED OUT: This prevents the GUI from loading because process_start() never returns.
       Uncomment this block only if you want to test user mode isolation specifically. */
    
    // terminal_writeline("[..] Setting up user mode test process...");
    // process_init();
    // extern void usertest_main(void);
    // process_t* proc = process_create(usertest_main);
    // if (proc) process_start(proc); 

    /* Initialize mouse once before entering GUI loop to avoid resetting controller on re-entry */
    mouse_init();
    terminal_writeline("[OK] Mouse (PS/2 IRQ12)");

gui_start:
    while (1) {
        /* ── GUI initialisation ──────────────────────────── */
        terminal_writeline("[OK] Entering GUI...");

        /* Init window manager — draws desktop, taskbar */
        wm_init();

        /* ─── Launch default apps ───────────────────────── */
        wm_launch_fileman();   /* Top-left */
        wm_launch_settings();  /* Center */
        wm_launch_diskmgr();   /* Right side */
        wm_launch_notepad();   /* Notepad */
        wm_launch_terminal();  /* Terminal shell */
        /* wm_launch_editor(NULL); */ /* Text Editor - commented out as it's text-mode only */

        extern void browser_launch(void);
        browser_launch();      /* Web Browser */

        /* ── GUI idle loop ───────────────────────────────── */
        while (1) {
            if (shell_mode_request == 1) {
                shell_mode_request = 0;
                break; /* exit GUI loop and go to tty mode */
            }

            __asm__ volatile ("hlt");

            /* Drain keyboard → events (wm_tick handles dispatch) */
            /* Mouse events come from IRQ12 → event_push (already wired) */

            /* Network */
            ethernet_poll();

            /* Window manager tick: process events, repaint, flip */
            wm_tick();
        }

        /* Switch to text mode shell */
        vga_enter_text_mode();
        terminal_clear();
        shell_run_text_mode();
        vga_exit_text_mode();

        /* Re-enter GUI after (gui command in text shell). */
        goto gui_start;
    }
}
