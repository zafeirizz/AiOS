#include <stdint.h>
#include <stddef.h>
#include "../../include/fb.h"
#include "../../include/heap.h" // Χρειαζόμαστε την kmalloc για τον buffer

// Τιμές από την entry.asm
extern uint32_t fb_phys_addr;
extern uint32_t fb_pitch_bytes;
extern uint32_t fb_width_px;
extern uint32_t fb_height_px;
extern uint32_t fb_bpp_val;

uint32_t* fb_addr = NULL;
uint32_t* fb_back = NULL;        // Ο buffer που έλειπε
int fb_initialized = 0;

void fb_init(void) {
    if (fb_phys_addr == 0) return;

    // Πραγματική διεύθυνση οθόνης
    fb_addr = (uint32_t*)(uintptr_t)fb_phys_addr;

    // Δημιουργία Back Buffer στη RAM (Double Buffering)
    // Μέγεθος = Πλάτος * Ύψος * 4 bytes (για 32-bit χρώμα)
    uint32_t size = fb_width_px * fb_height_px * 4;
    fb_back = (uint32_t*)kmalloc(size);

    if (fb_back == NULL) {
        // Αν αποτύχει η kmalloc, δούλευε απευθείας στην οθόνη (θα τρεμοπαίζει λίγο)
        fb_back = fb_addr;
    }

    fb_initialized = 1;
    fb_clear(0xFF000000); // Καθαρισμός buffer
    fb_flip();            // Εμφάνιση στην οθόνη
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_initialized || x >= fb_width_px || y >= fb_height_px) return;
    
    // Ζωγραφίζουμε ΠΑΝΤΑ στον fb_back
    uint32_t offset = y * (fb_pitch_bytes >> 2) + x;
    fb_back[offset] = color;
}

void fb_flip(void) {
    if (!fb_initialized || fb_back == fb_addr) return;

    // Αντιγραφή του back buffer στην πραγματική οθόνη
    uint32_t size_pixels = (fb_pitch_bytes >> 2) * fb_height_px;
    for (uint32_t i = 0; i < size_pixels; i++) {
        fb_addr[i] = fb_back[i];
    }
}

void fb_clear(uint32_t color) {
    if (!fb_initialized) return;
    uint32_t size_pixels = (fb_pitch_bytes >> 2) * fb_height_px;
    for (uint32_t i = 0; i < size_pixels; i++) {
        fb_back[i] = color;
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            fb_put_pixel(x + j, y + i, color);
        }
    }
}
