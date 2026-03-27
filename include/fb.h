#ifndef FB_H
#define FB_H

#include <stdint.h>

#define FB_WIDTH   1024
#define FB_HEIGHT  768
#define FB_BPP     32
#define FB_PITCH   (FB_WIDTH * (FB_BPP / 8))

void fb_init(void);

/* Framebuffer pointer — set by fb_init() from multiboot2 tag */
extern uint32_t* fb_addr;

/* Back buffer pointer — allocated from heap by fb_init() */
extern uint32_t* fb_back;

/* Inline helper: write to back buffer */
static inline void fb_put(int x, int y, uint32_t color) {
    if ((unsigned)x < FB_WIDTH && (unsigned)y < FB_HEIGHT)
        fb_back[y * FB_WIDTH + x] = color;
}

void fb_clear(uint32_t color);
void fb_flip(void);

#endif
