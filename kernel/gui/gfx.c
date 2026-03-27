#include <stdint.h>
#include "../../include/gfx.h"
#include "../../include/fb.h"
#include "../../include/font8x16.h"
#include "../../include/string.h"

/* ── Clip ────────────────────────────────────────────── */
static int clip_x=0,clip_y=0,clip_w=FB_WIDTH,clip_h=FB_HEIGHT;
void gfx_set_clip(int x,int y,int w,int h){clip_x=x;clip_y=y;clip_w=w;clip_h=h;}
void gfx_clear_clip(void){clip_x=0;clip_y=0;clip_w=FB_WIDTH;clip_h=FB_HEIGHT;}
static inline int in_clip(int x,int y){
    return x>=clip_x&&x<clip_x+clip_w&&y>=clip_y&&y<clip_y+clip_h;
}
static inline void put(int x,int y,uint32_t c){
    if(in_clip(x,y)) fb_back[y*FB_WIDTH+x]=c;
}
static inline uint32_t get(int x,int y){
    if((unsigned)x<FB_WIDTH&&(unsigned)y<FB_HEIGHT) return fb_back[y*FB_WIDTH+x];
    return 0;
}

/* ── Primitives ──────────────────────────────────────── */
void gfx_pixel(int x,int y,uint32_t c){put(x,y,c);}

void gfx_hline(int x,int y,int w,uint32_t c){
    for(int i=0;i<w;i++) put(x+i,y,c);
}
void gfx_vline(int x,int y,int h,uint32_t c){
    for(int i=0;i<h;i++) put(x,y+i,c);
}
void gfx_rect(int x,int y,int w,int h,uint32_t c){
    gfx_hline(x,y,w,c); gfx_hline(x,y+h-1,w,c);
    gfx_vline(x,y,h,c); gfx_vline(x+w-1,y,h,c);
}
void gfx_rect_fill(int x,int y,int w,int h,uint32_t c){
    for(int row=y;row<y+h;row++) gfx_hline(x,row,w,c);
}

void gfx_rect_rounded_fill(int x,int y,int w,int h,int r,uint32_t c){
    gfx_rect_fill(x+r,y,w-2*r,r,c);
    gfx_rect_fill(x,y+r,w,h-2*r,c);
    gfx_rect_fill(x+r,y+h-r,w-2*r,r,c);
    for(int dy=0;dy<r;dy++)
        for(int dx=0;dx<r;dx++)
            if(dx*dx+dy*dy<=r*r){
                put(x+r-1-dx,y+r-1-dy,c);
                put(x+w-r+dx,y+r-1-dy,c);
                put(x+r-1-dx,y+h-r+dy,c);
                put(x+w-r+dx,y+h-r+dy,c);
            }
}

void gfx_rect_rounded(int x,int y,int w,int h,int r,uint32_t c){
    gfx_hline(x+r,y,w-2*r,c); gfx_hline(x+r,y+h-1,w-2*r,c);
    gfx_vline(x,y+r,h-2*r,c); gfx_vline(x+w-1,y+r,h-2*r,c);
    for(int i=0;i<r;i++){
        int j=(int)((float)i*0.7f);
        put(x+r-1-j,y+r-1-i,c); put(x+w-r+j,y+r-1-i,c);
        put(x+r-1-j,y+h-r+i,c); put(x+w-r+j,y+h-r+i,c);
    }
}

void gfx_line(int x0,int y0,int x1,int y1,uint32_t c){
    int dx=x1-x0,dy=y1-y0;
    int ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    int sx=dx<0?-1:1,sy=dy<0?-1:1;
    int err=ax-ay;
    while(1){
        put(x0,y0,c);
        if(x0==x1&&y0==y1) break;
        int e2=2*err;
        if(e2>-ay){err-=ay;x0+=sx;}
        if(e2<ax) {err+=ax;y0+=sy;}
    }
}

void gfx_circle_fill(int cx,int cy,int r,uint32_t c){
    for(int y=-r;y<=r;y++)
        for(int x=-r;x<=r;x++)
            if(x*x+y*y<=r*r) put(cx+x,cy+y,c);
}

/* ── AiOS decorative primitives ──────────────────── */

/* Corner brackets: only draw the L-shaped corners of a rectangle.
 * sz = length of each arm of the bracket. */
void gfx_corner_brackets(int x,int y,int w,int h,int sz,uint32_t c){
    /* top-left */
    gfx_hline(x,y,sz,c); gfx_vline(x,y,sz,c);
    /* top-right */
    gfx_hline(x+w-sz,y,sz,c); gfx_vline(x+w-1,y,sz,c);
    /* bottom-left */
    gfx_hline(x,y+h-1,sz,c); gfx_vline(x,y+h-sz,sz,c);
    /* bottom-right */
    gfx_hline(x+w-sz,y+h-1,sz,c); gfx_vline(x+w-1,y+h-sz,sz,c);
}

/* Dashed horizontal line */
void gfx_dashed_hline(int x,int y,int w,int dash,int gap,uint32_t c){
    int i=0;
    while(i<w){
        for(int d=0;d<dash&&i<w;d++,i++) put(x+i,y,c);
        i+=gap;
    }
}

/* Scanline overlay: mix every other row pixel with 50% black.
 * Creates a CRT-like texture over any region. */
void gfx_scanlines(int x,int y,int w,int h){
    for(int row=y;row<y+h;row+=2){
        for(int col=x;col<x+w;col++){
            uint32_t p=get(col,row);
            /* Halve each channel */
            uint32_t r=((p>>16)&0xFF)>>1;
            uint32_t g=((p>>8)&0xFF)>>1;
            uint32_t b=(p&0xFF)>>1;
            put(col,row,(r<<16)|(g<<8)|b);
        }
    }
}

/* Tab bar with sharp-angle notch on right side */
void gfx_tab_bar(int x,int y,int w,int h,uint32_t bg,uint32_t border){
    gfx_rect_fill(x,y,w,h,bg);
    /* bottom border */
    gfx_hline(x,y+h-1,w,border);
    /* left edge accent line */
    gfx_vline(x,y,h,border);
    /* right-angled corner marks */
    gfx_hline(x,y,6,border);
    gfx_hline(x+w-6,y,6,border);
}

/* ── Text ─────────────────────────────────────────────── */
void gfx_text(int x,int y,const char* str,uint32_t fg){
    for(int i=0;str[i];i++){
        unsigned char ch=(unsigned char)str[i];
        if(ch>=128) ch='?';
        const unsigned char* glyph=font8x16[ch];
        for(int row=0;row<GFX_CHAR_H;row++){
            unsigned char bits=glyph[row];
            for(int col=0;col<GFX_CHAR_W;col++)
                if(bits&(0x80>>col)) put(x+i*GFX_CHAR_W+col,y+row,fg);
        }
    }
}

void gfx_text_bg(int x,int y,const char* str,uint32_t fg,uint32_t bg){
    for(int i=0;str[i];i++){
        unsigned char ch=(unsigned char)str[i];
        if(ch>=128) ch='?';
        const unsigned char* glyph=font8x16[ch];
        for(int row=0;row<GFX_CHAR_H;row++){
            unsigned char bits=glyph[row];
            for(int col=0;col<GFX_CHAR_W;col++)
                put(x+i*GFX_CHAR_W+col,y+row,(bits&(0x80>>col))?fg:bg);
        }
    }
}

int gfx_text_width(const char* str){int n=0;while(str[n])n++;return n*GFX_CHAR_W;}

void gfx_blit(int dx,int dy,int w,int h,const uint32_t* src,int src_stride){
    for(int y=0;y<h;y++)
        for(int x=0;x<w;x++)
            put(dx+x,dy+y,src[y*src_stride+x]);
}
