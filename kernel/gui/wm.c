#include <stdint.h>
#include "../../include/wm.h"
#include "../../include/gfx.h"
#include "../../include/fb.h"
#include "../../include/event.h"
#include "../../include/mouse.h"
#include "../../include/keyboard.h"
#include "../../include/string.h"
#include "../../include/ui.h"
#include "../../include/heap.h"
#include "../../include/editor.h"
#include "../../include/fileman.h"
#include "../../include/settings.h"
#include "../../include/diskmgr.h"
#include "../../include/notepad.h"
#include "../../include/calculator.h"
#include "../../include/ip.h"
#include "../../include/rtl8139.h"
#include "../../include/arp.h"
#include "../../include/timer.h"

extern void browser_launch(void);

/* ── Window pool ──────────────────────────────────────── */
static window_t windows[WM_MAX_WINDOWS];
static int      nwin = 0;
static int      zorder[WM_MAX_WINDOWS];
static int      cur_px = 512, cur_py = 384;

/* ── Layout constants ─────────────────────────────────── *
 *  QifshaOS uses a LEFT SIDEBAR launcher, no top bar.     *
 *  Title bar is a LEFT VERTICAL STRIP (not horizontal).   *
 *  Window chrome is bracket-style, not box-style.         *
 * ─────────────────────────────────────────────────────── */
#define SIDEBAR_W     52   /* launcher rail width          */
#define TITLESTRIP_W  20   /* vertical title strip width   */
#define WIN_BORDER     1
#define WM_TITLE_H    24   /* kept for wm_client_rect compat */

/* ── Crosshair cursor ─────────────────────────────────── */
static void draw_cursor(int x, int y) {
    uint32_t c = COLOR_ACCENT;
    /* clear center area */
    gfx_pixel(x-1,y,COLOR_BG); gfx_pixel(x+1,y,COLOR_BG);
    gfx_pixel(x,y-1,COLOR_BG); gfx_pixel(x,y+1,COLOR_BG);
    /* outer bracket marks */
    gfx_hline(x-6,y,4,c); gfx_hline(x+3,y,4,c);
    gfx_vline(x,y-6,4,c); gfx_vline(x,y+3,4,c);
    /* center dot */
    gfx_pixel(x,y,c);
}

/* ── App table ────────────────────────────────────────── */
typedef struct { const char* lbl; int (*fn)(void); } app_t;

void wm_launch_networkmgr(void);
static int act_fileman(void)  { wm_launch_fileman();  return 0; }
static int act_terminal(void) { wm_launch_terminal(); return 0; }
static int act_browser(void)  { browser_launch();     return 0; }
static int act_calc(void)     { calculator_launch();  return 0; }
static int act_notepad(void)  { wm_launch_notepad();  return 0; }
static int act_network(void)  { wm_launch_networkmgr(); return 0; }
static int act_settings(void) { wm_launch_settings(); return 0; }
static int act_diskmgr(void)  { wm_launch_diskmgr();  return 0; }

#define NUM_APPS 8
static app_t apps[NUM_APPS] = {
    { "FM", act_fileman  },
    { "TT", act_terminal },
    { "WB", act_browser  },
    { "CA", act_calc     },
    { "NP", act_notepad  },
    { "NW", act_network  },
    { "ST", act_settings },
    { "DM", act_diskmgr  },
};

/* ── Sidebar launcher ─────────────────────────────────── *
 *  Vertical strip on the left. Each app = a square tile   *
 *  with a 2-letter monogram and a teal bracket border.    *
 * ─────────────────────────────────────────────────────── */
#define TILE_SZ   44
#define TILE_PAD   4

static int sidebar_hover = -1;

/* Returns index of app tile hit, or -1 */
static int sidebar_hit(int mx, int my) {
    if (mx < 0 || mx >= SIDEBAR_W) return -1;
    /* Account for the 36px offset used in draw_sidebar (monogram area) */
    int idx = (my - TILE_PAD - 36) / (TILE_SZ + TILE_PAD);
    if (idx < 0 || idx >= NUM_APPS) return -1;
    int ty = TILE_PAD + 36 + idx * (TILE_SZ + TILE_PAD);
    if (my < ty || my >= ty + TILE_SZ) return -1;
    return idx;
}

static void draw_sidebar(void) {
    /* Rail background */
    gfx_rect_fill(0, 0, SIDEBAR_W, FB_HEIGHT, RGB(10,12,22));
    /* Right edge line */
    gfx_vline(SIDEBAR_W-1, 0, FB_HEIGHT, COLOR_BORDER);
    /* Tiny tick marks every tile */
    for (int i = 0; i <= NUM_APPS; i++) {
        int ty = TILE_PAD + i*(TILE_SZ+TILE_PAD);
        gfx_hline(SIDEBAR_W-5, ty, 4, COLOR_BORDER_DIM);
    }

    /* OS monogram at top */
    gfx_text(8,  8,  "Q",  COLOR_ACCENT);
    gfx_text(16, 8,  "i",  COLOR_ACCENT2);
    gfx_text(24, 8,  "fs", COLOR_TEXT_L);
    gfx_hline(4, 28, SIDEBAR_W-8, COLOR_BORDER_DIM);

    for (int i = 0; i < NUM_APPS; i++) {
        int ty = TILE_PAD + 36 + i*(TILE_SZ+TILE_PAD);
        int hov = (i == sidebar_hover);
        uint32_t bg     = hov ? RGB(0,40,36)  : RGB(14,18,30);
        uint32_t border = hov ? COLOR_ACCENT  : COLOR_BORDER_DIM;
        uint32_t fg     = hov ? COLOR_TEXT_HI : COLOR_TEXT_L;

        gfx_rect_fill(4, ty, TILE_SZ-8, TILE_SZ, bg);
        /* Corner brackets instead of full box */
        gfx_corner_brackets(4, ty, TILE_SZ-8, TILE_SZ, 6, border);

        /* 2-letter monogram, centred */
        int lw = gfx_text_width(apps[i].lbl);
        gfx_text(4+(TILE_SZ-8-lw)/2, ty+(TILE_SZ-GFX_CHAR_H)/2,
                 apps[i].lbl, fg);

        /* Active dot if hovered */
        if (hov) {
            gfx_hline(SIDEBAR_W-4, ty+TILE_SZ/2-1, 3, COLOR_ACCENT);
        }
    }

    /* Version tag at bottom */
    gfx_text(6, FB_HEIGHT-20, "v1.0", COLOR_BORDER_DIM);
}

/* ── Status bar — bottom ──────────────────────────────── *
 *  Single line: system stats, clock, focused window title *
 *  Uses dashed separators rather than solid lines.        *
 * ─────────────────────────────────────────────────────── */
#define STATUSBAR_H  18
#define STATUSBAR_Y  (FB_HEIGHT - STATUSBAR_H)

static void itoa_dec(int v, char* out) {
    if(!v){out[0]='0';out[1]='\0';return;}
    char t[12]; int i=11; t[i]=0;
    while(v>0){t[--i]='0'+(v%10);v/=10;}
    int j=0; while(t[i]) out[j++]=t[i++]; out[j]='\0';
}

static void draw_statusbar(void) {
    gfx_rect_fill(SIDEBAR_W, STATUSBAR_Y, FB_WIDTH-SIDEBAR_W, STATUSBAR_H, RGB(8,10,20));
    gfx_hline(SIDEBAR_W, STATUSBAR_Y, FB_WIDTH-SIDEBAR_W, COLOR_BORDER_DIM);

    int x = SIDEBAR_W + 8;
    int y = STATUSBAR_Y + 1;

    /* Uptime */
    uint32_t secs = timer_get_ticks() / 100;
    char upstr[32];
    strcpy(upstr, "UP:");
    char tmp[8]; itoa_dec((int)(secs/60), tmp); strcat(upstr,tmp);
    strcat(upstr,"m");
    gfx_text(x, y, upstr, COLOR_BORDER_DIM); x += gfx_text_width(upstr)+8;

    /* Dashed separator */
    gfx_dashed_hline(x, y+7, 6, 2, 2, COLOR_BORDER_DIM); x += 12;

    /* Window count */
    char wstr[12]; strcpy(wstr,"WIN:");
    itoa_dec(nwin, tmp); strcat(wstr,tmp);
    gfx_text(x, y, wstr, COLOR_TEXT_L); x += gfx_text_width(wstr)+8;

    gfx_dashed_hline(x, y+7, 6, 2, 2, COLOR_BORDER_DIM); x += 12;

    /* Focused window title */
    for(int i=nwin-1;i>=0;i--){
        window_t* w=&windows[zorder[i]];
        if(!w->visible||!w->focused) continue;
        gfx_text(x, y, ">", COLOR_ACCENT2);
        gfx_text(x+10, y, w->title, COLOR_TEXT_HI);
        break;
    }

    /* Clock right-aligned */
    uint32_t mins=(secs/60)%60, hrs=(secs/3600)%24;
    char clk[8];
    clk[0]='0'+(hrs/10); clk[1]='0'+(hrs%10);
    clk[2]=':';
    clk[3]='0'+(mins/10); clk[4]='0'+(mins%10);
    clk[5]='\0';
    gfx_text(FB_WIDTH-48, y, clk, COLOR_ACCENT);
}

/* ── Desktop pattern ──────────────────────────────────── *
 *  Sparse dot-grid — gives a "graph paper" feel without   *
 *  looking like any mainstream OS background.             *
 * ─────────────────────────────────────────────────────── */
static void draw_desktop_bg(void) {
    gfx_rect_fill(SIDEBAR_W, 0,
                  FB_WIDTH-SIDEBAR_W, FB_HEIGHT-STATUSBAR_H, COLOR_BG);

    /* Dot grid every 32 px */
    uint32_t dot = RGB(0, 35, 30);
    for(int y=16; y<FB_HEIGHT-STATUSBAR_H; y+=32)
        for(int x=SIDEBAR_W+16; x<FB_WIDTH; x+=32)
            gfx_pixel(x, y, dot);
}

/* ── Window chrome ────────────────────────────────────── *
 *  No rounded corners. No gradient titlebar.             *
 *  Instead:                                              *
 *   - Corner brackets mark the window boundary           *
 *   - A LEFT vertical strip holds the title (rotated)    *
 *     and the close/collapse controls                    *
 *   - Focused = teal brackets; unfocused = dim brackets  *
 * ─────────────────────────────────────────────────────── */

/* Control buttons inside the left strip — stacked vertically */
#define CTRL_SZ   14
#define CTRL_X    3    /* relative to window->x */

static int ctrl_close_y(const window_t* w) { return w->y + 6; }
static int ctrl_min_y  (const window_t* w) { return w->y + 6 + CTRL_SZ + 4; }

static int in_ctrl_close(const window_t* w, int mx, int my) {
    int cy = ctrl_close_y(w);
    return mx>=w->x+CTRL_X && mx<w->x+CTRL_X+CTRL_SZ &&
           my>=cy && my<cy+CTRL_SZ;
}
static int in_ctrl_min(const window_t* w, int mx, int my) {
    int cy = ctrl_min_y(w);
    return mx>=w->x+CTRL_X && mx<w->x+CTRL_X+CTRL_SZ &&
           my>=cy && my<cy+CTRL_SZ;
}
static int in_titlestrip(const window_t* w, int mx, int my) {
    return mx>=w->x && mx<w->x+TITLESTRIP_W &&
           my>=w->y && my<w->y+w->h;
}

static void draw_chrome(window_t* win) {
    uint32_t brk = win->focused ? COLOR_BORDER     : COLOR_BORDER_DIM;
    uint32_t acc = win->focused ? COLOR_ACCENT      : COLOR_BORDER_DIM;
    uint32_t acc2= win->focused ? COLOR_ACCENT2     : COLOR_BORDER_DIM;

    /* Outer corner brackets */
    gfx_corner_brackets(win->x, win->y, win->w, win->h, 10, brk);

    /* Left vertical strip */
    gfx_rect_fill(win->x, win->y, TITLESTRIP_W, win->h, RGB(10,14,24));
    gfx_vline(win->x+TITLESTRIP_W-1, win->y, win->h, brk);

    /* Close control: square bracket [] style */
    int cly = ctrl_close_y(win);
    gfx_rect(win->x+CTRL_X, cly, CTRL_SZ, CTRL_SZ, acc);
    gfx_line(win->x+CTRL_X+3, cly+3,
             win->x+CTRL_X+CTRL_SZ-4, cly+CTRL_SZ-4, acc);
    gfx_line(win->x+CTRL_X+CTRL_SZ-4, cly+3,
             win->x+CTRL_X+3, cly+CTRL_SZ-4, acc);

    /* Minimise control: just a horizontal dash */
    int mly = ctrl_min_y(win);
    gfx_rect(win->x+CTRL_X, mly, CTRL_SZ, CTRL_SZ, acc2);
    gfx_hline(win->x+CTRL_X+3, mly+CTRL_SZ/2, CTRL_SZ-6, acc2);

    /* Title — rendered vertically down the strip */
    /* We can't rotate text, so we write it horizontally  */
    /* in a clipped column, clipping naturally chops it.  */
    gfx_set_clip(win->x, win->y+40, TITLESTRIP_W-2, win->h-44);
    /* Write title characters downward, one per row */
    const char* t = win->title;
    int ty = win->y + 44;
    for(int i=0; t[i] && ty+GFX_CHAR_H < win->y+win->h-4; i++, ty+=GFX_CHAR_H) {
        char ch[2]={t[i],0};
        gfx_text(win->x+2, ty, ch,
                 win->focused ? COLOR_TEXT_L : COLOR_BORDER_DIM);
    }
    gfx_clear_clip();

    /* Top edge between strip end and window right */
    gfx_hline(win->x+TITLESTRIP_W, win->y, win->w-TITLESTRIP_W, brk);
    /* Bottom edge */
    gfx_hline(win->x, win->y+win->h-1, win->w, brk);
    /* Right edge */
    gfx_vline(win->x+win->w-1, win->y, win->h, brk);

    /* Dashed accent below top edge (decoration only) */
    if(win->focused)
        gfx_dashed_hline(win->x+TITLESTRIP_W, win->y+2,
                         win->w-TITLESTRIP_W, 4, 4, RGB(0,60,55));
}

static void draw_window(window_t* win) {
    if(!win->visible) return;
    int cx,cy,cw,ch;
    wm_client_rect(win,&cx,&cy,&cw,&ch);
    /* Client fill — dark */
    gfx_rect_fill(cx,cy,cw,ch,COLOR_WINBG);
    /* Subtle scanlines across entire client area */
    gfx_set_clip(cx,cy,cw,ch);
    if(win->on_draw) win->on_draw(win,cx,cy,cw,ch);
    gfx_scanlines(cx,cy,cw,ch);
    gfx_clear_clip();
    draw_chrome(win);
    win->dirty=0;
}

/* ── Init ─────────────────────────────────────────────── */
void wm_init(void) {
    memset(windows,0,sizeof(windows));
    for(int i=0;i<WM_MAX_WINDOWS;i++) zorder[i]=i;
    nwin=0;
    draw_desktop_bg();
    draw_sidebar();
    draw_statusbar();
    fb_flip();
}

/* ── Create ───────────────────────────────────────────── */
window_t* wm_create(const char* title,int x,int y,int w,int h,
                    wm_draw_fn draw,wm_event_fn event){
    if(nwin>=WM_MAX_WINDOWS) return (void*)0;
    window_t* win=&windows[nwin];
    memset(win,0,sizeof(*win));
    /* Offset by sidebar so apps don't need to know */
    win->x=x+SIDEBAR_W; win->y=y; win->w=w; win->h=h;
    win->visible=1; win->dirty=1;
    win->on_draw=draw; win->on_event=event;
    int i=0; while(title[i]&&i<63){win->title[i]=title[i];i++;} win->title[i]='\0';
    zorder[nwin]=nwin; nwin++;
    wm_focus(win);
    return win;
}

void wm_client_rect(const window_t* win,int* cx,int* cy,int* cw,int* ch){
    *cx=win->x+TITLESTRIP_W+WIN_BORDER;
    *cy=win->y+WIN_BORDER;
    *cw=win->w-TITLESTRIP_W-2*WIN_BORDER;
    *ch=win->h-2*WIN_BORDER;
}

void wm_invalidate(window_t* win){win->dirty=1;}

void wm_focus(window_t* win){
    for(int i=0;i<nwin;i++) windows[i].focused=(&windows[i]==win);
    int idx=(int)(win-windows),pos=-1;
    for(int i=0;i<nwin;i++) if(zorder[i]==idx){pos=i;break;}
    if(pos>=0){
        for(int i=pos;i<nwin-1;i++) zorder[i]=zorder[i+1];
        zorder[nwin-1]=idx;
    }
    win->dirty=1;
}

void wm_close(window_t* win){win->visible=0;}

/* ── App launchers ─────────────────────────────────────── */
void wm_launch_editor(const char* f){editor_open(f);}
void wm_launch_fileman(void){fileman_launch();}
void wm_launch_settings(void){settings_launch();}
void wm_launch_diskmgr(void){diskmgr_launch();}
void wm_launch_notepad(void){notepad_launch();}
void wm_launch_browser(void){browser_launch();}

static inline void outb(uint16_t port,uint8_t val){
    __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}
void system_reboot(void){outb(0x64,0xFE);for(;;)__asm__ volatile("hlt");}

/* ── Network manager app ──────────────────────────────── */
static void itoa_ip_part(uint32_t v,char* out){itoa_dec((int)v,out);}
static void ip_to_str(uint32_t ip,char* b){
    char p[4];
    itoa_ip_part(ip&0xFF,p);         strcpy(b,p); strcat(b,".");
    itoa_ip_part((ip>>8)&0xFF,p);    strcat(b,p); strcat(b,".");
    itoa_ip_part((ip>>16)&0xFF,p);   strcat(b,p); strcat(b,".");
    itoa_ip_part((ip>>24)&0xFF,p);   strcat(b,p);
}
static void mac_to_str(const uint8_t* mac,char* b){
    const char* h="0123456789ABCDEF";
    int p=0;
    for(int i=0;i<6;i++){b[p++]=h[(mac[i]>>4)&0xF];b[p++]=h[mac[i]&0xF];if(i<5)b[p++]=':';}
    b[p]='\0';
}

typedef struct{ui_button_t btn;char status[256];}netmgr_t;
static void nm_draw(window_t* w,int cx,int cy,int cw,int ch){
    netmgr_t* s=(netmgr_t*)w->app_data; if(!s)return;
    gfx_rect_fill(cx,cy,cw,ch,COLOR_WINBG);
    gfx_corner_brackets(cx+2,cy+2,cw-4,ch-4,8,COLOR_BORDER_DIM);
    gfx_text(cx+10,cy+8,"NETWORK",COLOR_ACCENT);
    gfx_dashed_hline(cx+10,cy+26,cw-20,4,4,COLOR_BORDER_DIM);
    gfx_text(cx+10,cy+34,s->status,COLOR_TEXT_L);
    ui_button_t b=s->btn;b.x+=cx;b.y+=cy;ui_button_draw(&b);
}
static void nm_event(window_t* w,const event_t* e){
    netmgr_t* s=(netmgr_t*)w->app_data; if(!s)return;
    int cx,cy,cw,ch; wm_client_rect(w,&cx,&cy,&cw,&ch);
    ui_button_event(&s->btn,e,cx,cy);
    if(s->btn.clicked){
        s->btn.clicked=0;
        char ip[20],gw[20],nm[20],mac[20];
        ip_to_str(ip_our_ip,ip); ip_to_str(ip_gateway,gw); ip_to_str(ip_netmask,nm);
        const uint8_t* m=rtl8139_mac();
        if(m) mac_to_str(m,mac); else strcpy(mac,"N/A");
        strcpy(s->status,"IP:  "); strcat(s->status,ip);
        strcat(s->status,"\nGW:  "); strcat(s->status,gw);
        strcat(s->status,"\nNM:  "); strcat(s->status,nm);
        strcat(s->status,"\nMAC: "); strcat(s->status,mac);
        wm_invalidate(w);
    }
}
void wm_launch_networkmgr(void){
    netmgr_t* s=(netmgr_t*)kmalloc(sizeof(netmgr_t));
    if(!s) {
        return;
    }
    memset(s,0,sizeof(netmgr_t));

    char ip[20],gw[20],nm[20];
    ip_to_str(ip_our_ip,ip); ip_to_str(ip_gateway,gw); ip_to_str(ip_netmask,nm);
    strcpy(s->status,"IP:  "); strcat(s->status,ip);
    strcat(s->status,"\nGW:  "); strcat(s->status,gw);
    strcat(s->status,"\nNM:  "); strcat(s->status,nm);
    s->btn=ui_button_create(10,110,80,22,"Refresh");
    window_t* w=wm_create("Network",60,60,280,170,nm_draw,nm_event);
    if(w) w->app_data=s;
}

/* ── Hit testing ──────────────────────────────────────── */
static window_t* hit_window(int mx,int my){
    for(int i=nwin-1;i>=0;i--){
        window_t* w=&windows[zorder[i]];
        if(!w->visible)continue;
        if(mx>=w->x&&mx<w->x+w->w&&my>=w->y&&my<w->y+w->h) return w;
    }
    return (void*)0;
}

static window_t* drag_win=(void*)0;
static int drag_ox,drag_oy;

/* ── Main tick ────────────────────────────────────────── */
void wm_tick(void){
    event_t e;
    while(event_poll(&e)){
        if(e.type==EVT_MOUSE_MOVE){
            int mx=e.mouse.x,my=e.mouse.y;
            cur_px=mx; cur_py=my;
            sidebar_hover=sidebar_hit(mx,my);
            if(drag_win){
                drag_win->x=mx-drag_ox;
                drag_win->y=my-drag_oy;
                /* Clamp */
                if(drag_win->x<SIDEBAR_W) drag_win->x=SIDEBAR_W;
                if(drag_win->y<0) drag_win->y=0;
                if(drag_win->x+drag_win->w>FB_WIDTH)  drag_win->x=FB_WIDTH-drag_win->w;
                if(drag_win->y+drag_win->h>STATUSBAR_Y) drag_win->y=STATUSBAR_Y-drag_win->h;
                drag_win->dirty=1;
            } else {
                for(int i=nwin-1;i>=0;i--){
                    window_t* w=&windows[zorder[i]];
                    if(!w->visible||!w->focused)continue;
                    int cx,cy,cw,ch; wm_client_rect(w,&cx,&cy,&cw,&ch);
                    if(mx>=cx&&mx<cx+cw&&my>=cy&&my<cy+ch&&w->on_event){
                        event_t fwd=e; fwd.mouse.x=mx-cx; fwd.mouse.y=my-cy;
                        w->on_event(w,&fwd);
                    }
                    break;
                }
            }
        }
        else if(e.type==EVT_MOUSE_DOWN&&e.mouse.button==0){
            int mx=e.mouse.x,my=e.mouse.y;
            /* Sidebar tile click */
            int si=sidebar_hit(mx,my);
            if(si>=0){ if(apps[si].fn) apps[si].fn(); continue; }

            window_t* hit=hit_window(mx,my);
            if(hit){
                wm_focus(hit);
                if(in_ctrl_close(hit,mx,my)){
                    wm_close(hit);
                } else if(in_ctrl_min(hit,mx,my)){
                    hit->visible=0;
                } else if(in_titlestrip(hit,mx,my)){
                    drag_win=hit; drag_ox=mx-hit->x; drag_oy=my-hit->y;
                } else {
                    int cx,cy,cw,ch; wm_client_rect(hit,&cx,&cy,&cw,&ch);
                    if(hit->on_event){
                        event_t fwd=e; fwd.mouse.x=mx-cx; fwd.mouse.y=my-cy;
                        hit->on_event(hit,&fwd);
                    }
                }
            }
        }
        else if(e.type==EVT_MOUSE_UP){
            if(drag_win){drag_win=(void*)0;}
            for(int i=nwin-1;i>=0;i--){
                window_t* w=&windows[zorder[i]];
                if(!w->visible||!w->focused)continue;
                int cx,cy,cw,ch; wm_client_rect(w,&cx,&cy,&cw,&ch);
                if(w->on_event){
                    event_t fwd=e; fwd.mouse.x=e.mouse.x-cx; fwd.mouse.y=e.mouse.y-cy;
                    w->on_event(w,&fwd);
                }
                break;
            }
        }
        else if(e.type==EVT_KEY_DOWN){
            for(int i=nwin-1;i>=0;i--){
                window_t* w=&windows[zorder[i]];
                if(!w->visible||!w->focused)continue;
                if(w->on_event) w->on_event(w,&e);
                break;
            }
        }
    }

    /* Keyboard */
    char c;
    while((c=keyboard_poll())!=0){
        event_t ke; ke.type=EVT_KEY_DOWN; ke.key.c=c; ke.key.scancode=0;
        for(int i=nwin-1;i>=0;i--){
            window_t* w=&windows[zorder[i]];
            if(!w->visible||!w->focused)continue;
            if(w->on_event) w->on_event(w,&ke);
            break;
        }
    }

    /* ── Render frame ─────────────────────────────────── */
    draw_desktop_bg();

    /* Windows bottom-to-top */
    for(int i=0;i<nwin;i++){
        window_t* w=&windows[zorder[i]];
        if(w->visible) draw_window(w);
    }

    draw_sidebar();
    draw_statusbar();
    draw_cursor(cur_px,cur_py);
    fb_flip();
}
