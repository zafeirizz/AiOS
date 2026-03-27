#include <stdint.h>
#include <stddef.h>
#include "../../include/vga.h"
#include "../../include/fb.h"
#include "../../include/heap.h"

/* These are set by entry.asm from GRUB's multiboot2 framebuffer tag */
/* Defined in entry.asm */
extern uint32_t fb_phys_addr;
extern uint32_t fb_pitch;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_bpp;

uint32_t* fb_addr = NULL;
uint32_t* fb_back = NULL;
static int fb_initialized = 0;

void fb_init(void) {
    terminal_writeline("");
    terminal_writeline("=== Framebuffer Info ===");
    
    terminal_write("Address: 0x");
    terminal_write_hex(fb_phys_addr);
    terminal_writeline("");
    
    terminal_write("Width: ");
    terminal_write_dec(fb_width);
    terminal_writeline("");
    
    terminal_write("Height: ");
    terminal_write_dec(fb_height);
    terminal_writeline("");
    
    terminal_write("BPP: ");
    terminal_write_dec(fb_bpp);
    terminal_writeline("");
    
    terminal_write("Pitch: ");
    terminal_write_dec(fb_pitch);
    terminal_writeline("");
    
    if (fb_phys_addr != 0 && fb_width > 0 && fb_height > 0) {
        terminal_writeline("");
        terminal_writeline("Framebuffer detected!");
        fb_addr = (uint32_t*)fb_phys_addr;
        
        /* Allocate back buffer for double buffering */
        uint32_t size = fb_width * fb_height * 4;
        fb_back = (uint32_t*)kmalloc(size);
        if (!fb_back) fb_back = fb_addr; /* Fallback if alloc fails */
        
        fb_initialized = 1;
    } else {
        terminal_writeline("");
        terminal_writeline("No framebuffer detected!");
        terminal_writeline("Make sure grub.cfg has: set gfxpayload=1024x768x32");
        fb_initialized = 0;
    }
}

void fb_flip(void) {
    if (!fb_initialized || !fb_back || fb_back == fb_addr) return;
    
    /* Copy back buffer to front buffer */
    /* Note: Ideally use optimized memcpy, but loop works for now */
    uint32_t len = fb_width * fb_height;
    for (uint32_t i = 0; i < len; i++) {
        fb_addr[i] = fb_back[i];
    }
}

int fb_is_available(void) {
    return (fb_initialized && fb_back != NULL);
}

void fb_clear(uint32_t color) {
    if (!fb_is_available()) return;
    
    uint32_t* fb = fb_back;
    uint32_t total_pixels = fb_width * fb_height;
    
    for (uint32_t i = 0; i < total_pixels; i++) {
        fb[i] = color;
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb_is_available()) return;
    
    uint32_t* fb = fb_back;
    uint32_t pitch_words = fb_pitch / 4;
    uint32_t max_y = y + h;
    uint32_t max_x = x + w;
    
    if (max_y > fb_height) max_y = fb_height;
    if (max_x > fb_width) max_x = fb_width;
    
    for (uint32_t row = y; row < max_y; row++) {
        uint32_t offset = row * pitch_words;
        for (uint32_t col = x; col < max_x; col++) {
            fb[offset + col] = color;
        }
    }
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_is_available()) return;
    if (x >= fb_width || y >= fb_height) return;
    
    uint32_t* fb = fb_back;
    uint32_t pitch_words = fb_pitch / 4;
    fb[y * pitch_words + x] = color;
}
