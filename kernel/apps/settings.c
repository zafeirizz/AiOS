#include <stdint.h>
#include <string.h>
#include "../../include/settings.h"
#include "../../include/wm.h"
#include "../../include/ui.h"
#include "../../include/gfx.h"
#include "../../include/myfs.h"
#include "../../include/heap.h"
#include "../../include/ip.h"
#include "../../include/dns.h"
#include "../../include/rtl8139.h"
#include "../../include/e1000.h"
#include "../../include/string.h"

/* ── Panels ─────────────────────────────────────────────── */
#define PANEL_DISPLAY   0
#define PANEL_NETWORK   1
#define PANEL_SOUND     2
#define PANEL_SYSTEM    3
#define PANEL_ABOUT     4
#define NUM_PANELS      5

static const char* panel_names[NUM_PANELS] = {
    "Display", "Network", "Sound", "System", "About"
};

typedef struct {
    int  active_panel;

    /* Display */
    int  brightness;      /* 0-100 */
    int  theme;           /* 0=dark 1=light */

    /* Network */
    char ip_str[20];
    char gw_str[20];
    char nm_str[20];
    char dns_str[20];
    int  net_driver;      /* 0=rtl8139 1=e1000 */
    int  net_editing;     /* which field 0-3, -1=none */
    char net_edit_buf[20];

    /* Sound */
    int  vol;             /* 0-100 */
    int  muted;

    /* System */
    int  mouse_speed;     /* 1-5 */

    /* Nav + action buttons */
    ui_button_t nav[NUM_PANELS];
    ui_button_t btn_apply;
    ui_button_t btn_save;
    ui_button_t btn_brightness_up;
    ui_button_t btn_brightness_dn;
    ui_button_t btn_theme;
    ui_button_t btn_vol_up;
    ui_button_t btn_vol_dn;
    ui_button_t btn_mute;
    ui_button_t btn_mouse_up;
    ui_button_t btn_mouse_dn;
    ui_button_t btn_driver_rtl;
    ui_button_t btn_driver_e1k;
    ui_button_t btn_net_edit[4];
    ui_button_t btn_net_ok;
} settings_state_t;

/* ── Helpers ──────────────────────────────────────────────── */
static void int_to_str(int v, char* out) {
    if (v == 0) { out[0]='0'; out[1]='\0'; return; }
    char tmp[12]; int i=0;
    while(v>0){tmp[i++]='0'+(v%10);v/=10;}
    int j=0; while(i-->0) out[j++]=tmp[i]; out[j]='\0';
}

static void ip_uint_to_str(uint32_t ip, char* out) {
    /* ip is stored host byte order: LSB = first octet */
    char parts[4][4];
    int_to_str(ip & 0xFF,         parts[0]);
    int_to_str((ip>>8) & 0xFF,    parts[1]);
    int_to_str((ip>>16) & 0xFF,   parts[2]);
    int_to_str((ip>>24) & 0xFF,   parts[3]);
    strcpy(out, parts[0]); strcat(out,".");
    strcat(out,parts[1]);  strcat(out,".");
    strcat(out,parts[2]);  strcat(out,".");
    strcat(out,parts[3]);
}

/* ── Draw panel helpers ────────────────────────────────── */
static void draw_label_value(int cx, int y, const char* lbl, const char* val,
                              uint32_t vcol) {
    gfx_text(cx+10, y, lbl, COLOR_TEXT_L);
    gfx_text(cx+160, y, val, vcol);
}

static void draw_separator(int cx, int y, int cw) {
    gfx_hline(cx+6, y, cw-12, COLOR_BORDER);
}

/* ── Draw Display panel ─────────────────────────────────── */
static void draw_display(settings_state_t* s, int cx, int cy, int cw) {
    int y = cy + 10;
    gfx_text(cx+10, y, "Display Settings", COLOR_ACCENT); y += 20;
    draw_separator(cx, y, cw); y += 10;

    draw_label_value(cx, y, "Brightness:", "", COLOR_TEXT_L);
    char bstr[8]; int_to_str(s->brightness, bstr); strcat(bstr, "%");
    gfx_text(cx+160, y, bstr, COLOR_ACCENT);
    ui_button_t bu = s->btn_brightness_up; bu.x+=cx; bu.y+=cy; ui_button_draw(&bu);
    ui_button_t bd = s->btn_brightness_dn; bd.x+=cx; bd.y+=cy; ui_button_draw(&bd);
    y += 32;

    draw_label_value(cx, y, "Theme:", s->theme ? "Light" : "Dark", COLOR_ACCENT);
    ui_button_t bt = s->btn_theme; bt.x+=cx; bt.y+=cy; ui_button_draw(&bt);
    y += 32;

    draw_separator(cx, y, cw); y += 8;
    gfx_text(cx+10, y, "Color scheme preview:", COLOR_TEXT_L); y += 16;
    gfx_rect_fill(cx+10, y, 20, 12, COLOR_BG);      gfx_text(cx+34, y, "BG",      COLOR_TEXT_D);
    gfx_rect_fill(cx+90, y, 20, 12, COLOR_ACCENT);  gfx_text(cx+114,y, "Accent",  COLOR_TEXT_D);
    gfx_rect_fill(cx+170,y, 20, 12, COLOR_TITLEBAR);gfx_text(cx+194,y, "Title",   COLOR_TEXT_D);
}

/* ── Draw Network panel ────────────────────────────────── */
static void draw_network(settings_state_t* s, int cx, int cy, int cw) {
    int y = cy + 10;
    gfx_text(cx+10, y, "Network Settings", COLOR_ACCENT); y += 20;
    draw_separator(cx, y, cw); y += 10;

    /* Driver selection */
    gfx_text(cx+10, y, "Driver:", COLOR_TEXT_L);
    ui_button_t br = s->btn_driver_rtl; br.x+=cx; br.y+=cy;
    ui_button_t be = s->btn_driver_e1k; be.x+=cx; be.y+=cy;
    if (s->net_driver == 0) br.pressed = 1; else be.pressed = 1;
    ui_button_draw(&br);
    ui_button_draw(&be);
    y += 32;

    draw_separator(cx, y, cw); y += 8;

    const char* field_labels[] = { "IP Address:", "Gateway:", "Netmask:", "DNS Server:" };
    const char* field_vals[]   = { s->ip_str, s->gw_str, s->nm_str, s->dns_str };

    for (int i = 0; i < 4; i++) {
        gfx_text(cx+10, y, field_labels[i], COLOR_TEXT_L);
        if (s->net_editing == i) {
            /* Draw editable input box */
            gfx_rect_fill(cx+160, y-2, 140, 16, COLOR_WINBG);
            gfx_rect(cx+160, y-2, 140, 16, COLOR_ACCENT);
            gfx_text(cx+164, y, s->net_edit_buf, COLOR_TEXT_L);
            gfx_vline(cx+164+gfx_text_width(s->net_edit_buf), y, GFX_CHAR_H, COLOR_ACCENT);
        } else {
            gfx_text(cx+160, y, field_vals[i], COLOR_ACCENT);
        }
        ui_button_t eb = s->btn_net_edit[i]; eb.x+=cx; eb.y+=cy; ui_button_draw(&eb);
        y += 22;
    }

    if (s->net_editing >= 0) {
        ui_button_t ok = s->btn_net_ok; ok.x+=cx; ok.y+=cy; ui_button_draw(&ok);
    }
}

/* ── Draw Sound panel ──────────────────────────────────── */
static void draw_sound(settings_state_t* s, int cx, int cy, int cw) {
    int y = cy + 10;
    gfx_text(cx+10, y, "Sound Settings", COLOR_ACCENT); y += 20;
    draw_separator(cx, y, cw); y += 10;

    draw_label_value(cx, y, "Volume:", "", COLOR_TEXT_L);
    char vstr[8]; int_to_str(s->vol, vstr); strcat(vstr, "%");
    gfx_text(cx+160, y, vstr, COLOR_ACCENT);
    ui_button_t vu = s->btn_vol_up; vu.x+=cx; vu.y+=cy; ui_button_draw(&vu);
    ui_button_t vd = s->btn_vol_dn; vd.x+=cx; vd.y+=cy; ui_button_draw(&vd);
    y += 30;

    /* Volume bar */
    gfx_text(cx+10, y, "Level:", COLOR_TEXT_L);
    gfx_rect(cx+60, y+2, 200, 10, COLOR_BORDER);
    int fill = 200 * s->vol / 100;
    uint32_t vcol = s->muted ? COLOR_GRAY : (s->vol>70 ? COLOR_RED : COLOR_GREEN);
    if (fill > 0 && !s->muted) gfx_rect_fill(cx+61, y+3, fill, 8, vcol);
    y += 20;

    draw_label_value(cx, y, "Muted:", s->muted ? "Yes" : "No",
                     s->muted ? COLOR_RED : COLOR_GREEN);
    ui_button_t bm = s->btn_mute; bm.x+=cx; bm.y+=cy; ui_button_draw(&bm);
    y += 30;

    gfx_text(cx+10, y, "(PC speaker / no audio HW detected)", COLOR_GRAY);
}

/* ── Draw System panel ─────────────────────────────────── */
static void draw_system(settings_state_t* s, int cx, int cy, int cw) {
    int y = cy + 10;
    gfx_text(cx+10, y, "System Settings", COLOR_ACCENT); y += 20;
    draw_separator(cx, y, cw); y += 10;

    draw_label_value(cx, y, "Mouse Speed:", "", COLOR_TEXT_L);
    char ms[4]; int_to_str(s->mouse_speed, ms);
    gfx_text(cx+160, y, ms, COLOR_ACCENT);
    ui_button_t mu = s->btn_mouse_up; mu.x+=cx; mu.y+=cy; ui_button_draw(&mu);
    ui_button_t md = s->btn_mouse_dn; md.x+=cx; md.y+=cy; ui_button_draw(&md);
    y += 30;

    draw_separator(cx, y, cw); y += 8;
    gfx_text(cx+10, y, "Kernel: AiOS x86 (i686)", COLOR_TEXT_L); y += 16;
    gfx_text(cx+10, y, "Build:  ffreestanding -O2 -m32", COLOR_TEXT_L); y += 16;
    gfx_text(cx+10, y, "Heap:   16MB  Framebuffer: 1024x768", COLOR_TEXT_L);
}

/* ── Draw About panel ──────────────────────────────────── */
static void draw_about(int cx, int cy) {
    int y = cy + 10;
    gfx_text(cx+10, y, "About AIos", COLOR_ACCENT); y += 20;
    gfx_hline(cx+6, y, 360, COLOR_BORDER); y += 10;
    gfx_text(cx+10, y, "AiOS v1.0", COLOR_TEXT_L); y += 16;
    gfx_text(cx+10, y, "Architecture:  x86 (i686)", COLOR_TEXT_L); y += 16;
    gfx_text(cx+10, y, "Filesystems:   MyFS, FAT32, ZFSX", COLOR_TEXT_L); y += 16;
    gfx_text(cx+10, y, "Network:       RTL8139, e1000", COLOR_TEXT_L); y += 16;
    gfx_text(cx+10, y, "Stack:         ARP/IP/ICMP/UDP/TCP/DNS", COLOR_TEXT_L); y += 16;
    gfx_text(cx+10, y, "GUI:           Framebuffer WM (1024x768)", COLOR_TEXT_L); y += 16;
    gfx_text(cx+10, y, "Shell:         Built-in with VFS commands", COLOR_TEXT_L); y += 20;
    gfx_text(cx+10, y, "Made with love to the Holy Mary by zafeirizz", COLOR_GRAY);
}

/* ── Main draw ─────────────────────────────────────────── */
static void settings_draw(window_t* win, int cx, int cy, int cw, int ch) {
    settings_state_t* state = (settings_state_t*)win->app_data;
    if (!state) return;

    gfx_rect_fill(cx, cy, cw, ch, COLOR_WINBG);

    /* Left sidebar */
    int sb_w = 90;
    gfx_rect_fill(cx, cy, sb_w, ch, COLOR_TITLEBAR);
    gfx_rect_fill(cx+sb_w, cy, 1, ch, COLOR_BORDER);

    for (int i = 0; i < NUM_PANELS; i++) {
        int ny = cy + 10 + i * 30;
        uint32_t bg = (i == state->active_panel) ? COLOR_ACCENT : COLOR_TITLEBAR;
        gfx_rect_fill(cx, ny-2, sb_w, 22, bg);
        gfx_text(cx+8, ny+2, panel_names[i], COLOR_TEXT_D);
    }

    /* Content area */
    int px = cx + sb_w + 4;
    int pw = cw - sb_w - 4;

    switch (state->active_panel) {
        case PANEL_DISPLAY: draw_display(state, px, cy, pw); break;
        case PANEL_NETWORK: draw_network(state, px, cy, pw); break;
        case PANEL_SOUND:   draw_sound(state, px, cy, pw);   break;
        case PANEL_SYSTEM:  draw_system(state, px, cy, pw);  break;
        case PANEL_ABOUT:   draw_about(px, cy);               break;
    }

    /* Bottom buttons */
    ui_button_t ba = state->btn_apply; ba.x+=cx; ba.y+=cy; ui_button_draw(&ba);
    ui_button_t bs = state->btn_save;  bs.x+=cx; bs.y+=cy; ui_button_draw(&bs);
}

/* ── Event ─────────────────────────────────────────────── */
static void settings_event(window_t* win, const event_t* e) {
    settings_state_t* state = (settings_state_t*)win->app_data;
    if (!state) return;
    int cx, cy, cw, ch;
    wm_client_rect(win, &cx, &cy, &cw, &ch);

    /* Network field keyboard input */
    if (state->net_editing >= 0 && e->type == EVT_KEY_DOWN) {
        char c = e->key.c;
        if (c == '\n') {
            /* Commit edit */
            switch (state->net_editing) {
                case 0: strncpy(state->ip_str,  state->net_edit_buf, 19); break;
                case 1: strncpy(state->gw_str,  state->net_edit_buf, 19); break;
                case 2: strncpy(state->nm_str,  state->net_edit_buf, 19); break;
                case 3: strncpy(state->dns_str, state->net_edit_buf, 19); break;
            }
            state->net_editing = -1;
        } else if (c == 27) {
            state->net_editing = -1;
        } else if (c == '\b') {
            int l = (int)strlen(state->net_edit_buf);
            if (l > 0) state->net_edit_buf[l-1] = '\0';
        } else if ((c >= '0' && c <= '9') || c == '.') {
            int l = (int)strlen(state->net_edit_buf);
            if (l < 19) { state->net_edit_buf[l] = c; state->net_edit_buf[l+1] = '\0'; }
        }
        wm_invalidate(win);
    }

    /* Sidebar nav clicks */
    if (e->type == EVT_MOUSE_DOWN && e->mouse.button == 0) {
        int mx = e->mouse.x + cx, my = e->mouse.y + cy;
        if (mx >= cx && mx < cx + 90) {
            int idx = (my - cy - 8) / 30;
            if (idx >= 0 && idx < NUM_PANELS) {
                state->active_panel = idx;
                wm_invalidate(win);
            }
        }
    }

    /* Display */
    ui_button_event(&state->btn_brightness_up, e, 0, 0);
    ui_button_event(&state->btn_brightness_dn, e, 0, 0);
    ui_button_event(&state->btn_theme,         e, 0, 0);
    if (state->btn_brightness_up.clicked) {
        state->btn_brightness_up.clicked = 0;
        if (state->brightness < 100) state->brightness += 10;
        wm_invalidate(win);
    }
    if (state->btn_brightness_dn.clicked) {
        state->btn_brightness_dn.clicked = 0;
        if (state->brightness > 0) state->brightness -= 10;
        wm_invalidate(win);
    }
    if (state->btn_theme.clicked) {
        state->btn_theme.clicked = 0;
        state->theme = !state->theme;
        wm_invalidate(win);
    }

    /* Network */
    ui_button_event(&state->btn_driver_rtl, e, 0, 0);
    ui_button_event(&state->btn_driver_e1k, e, 0, 0);
    for (int i = 0; i < 4; i++) ui_button_event(&state->btn_net_edit[i], e, 0, 0);
    ui_button_event(&state->btn_net_ok, e, 0, 0);
    if (state->btn_driver_rtl.clicked) { state->btn_driver_rtl.clicked=0; state->net_driver=0; wm_invalidate(win); }
    if (state->btn_driver_e1k.clicked) { state->btn_driver_e1k.clicked=0; state->net_driver=1; wm_invalidate(win); }
    for (int i = 0; i < 4; i++) {
        if (state->btn_net_edit[i].clicked) {
            state->btn_net_edit[i].clicked = 0;
            state->net_editing = i;
            const char* vals[] = { state->ip_str, state->gw_str, state->nm_str, state->dns_str };
            strncpy(state->net_edit_buf, vals[i], 19);
            wm_invalidate(win);
        }
    }
    if (state->btn_net_ok.clicked) {
        state->btn_net_ok.clicked = 0;
        state->net_editing = -1;
        wm_invalidate(win);
    }

    /* Sound */
    ui_button_event(&state->btn_vol_up, e, 0, 0);
    ui_button_event(&state->btn_vol_dn, e, 0, 0);
    ui_button_event(&state->btn_mute,   e, 0, 0);
    if (state->btn_vol_up.clicked) { state->btn_vol_up.clicked=0; if(state->vol<100)state->vol+=10; wm_invalidate(win); }
    if (state->btn_vol_dn.clicked) { state->btn_vol_dn.clicked=0; if(state->vol>0)state->vol-=10; wm_invalidate(win); }
    if (state->btn_mute.clicked)   { state->btn_mute.clicked=0; state->muted=!state->muted; wm_invalidate(win); }

    /* System */
    ui_button_event(&state->btn_mouse_up, e, 0, 0);
    ui_button_event(&state->btn_mouse_dn, e, 0, 0);
    if (state->btn_mouse_up.clicked) { state->btn_mouse_up.clicked=0; if(state->mouse_speed<5)state->mouse_speed++; wm_invalidate(win); }
    if (state->btn_mouse_dn.clicked) { state->btn_mouse_dn.clicked=0; if(state->mouse_speed>1)state->mouse_speed--; wm_invalidate(win); }

    /* Apply / Save */
    ui_button_event(&state->btn_apply, e, 0, 0);
    ui_button_event(&state->btn_save,  e, 0, 0);
    if (state->btn_apply.clicked) {
        state->btn_apply.clicked = 0;
        /* Apply network config */
        uint32_t new_ip = 0, new_gw = 0, new_nm = 0, new_dns = 0;
        dns_parse_ip(state->ip_str,  &new_ip);
        dns_parse_ip(state->gw_str,  &new_gw);
        dns_parse_ip(state->nm_str,  &new_nm);
        dns_parse_ip(state->dns_str, &new_dns);
        ip_init(new_ip, new_gw, new_nm);
        if (new_dns) dns_set_server(new_dns);
        wm_invalidate(win);
    }
    if (state->btn_save.clicked) {
        state->btn_save.clicked = 0;
        /* Persist to MyFS as "settings.cfg" */
        char cfg[256];
        strcpy(cfg, "ip="); strcat(cfg, state->ip_str);   strcat(cfg, "\n");
        strcat(cfg, "gw="); strcat(cfg, state->gw_str);   strcat(cfg, "\n");
        strcat(cfg, "nm="); strcat(cfg, state->nm_str);   strcat(cfg, "\n");
        strcat(cfg, "dns=");strcat(cfg, state->dns_str);  strcat(cfg, "\n");
        myfs_write("settings.cfg", (const uint8_t*)cfg, (uint32_t)strlen(cfg));
        wm_invalidate(win);
    }
}

/* ── Launch ───────────────────────────────────────────────── */
int settings_launch(void) {
    settings_state_t* state =
        (settings_state_t*)kmalloc(sizeof(settings_state_t));
    if (!state) return -1;
    memset(state, 0, sizeof(settings_state_t));

    state->active_panel = PANEL_DISPLAY;
    state->brightness   = 80;
    state->theme        = 0;
    state->vol          = 60;
    state->mouse_speed  = 2;
    state->net_driver   = 0;
    state->net_editing  = -1;

    /* Populate network fields from live config */
    ip_uint_to_str(ip_our_ip,  state->ip_str);
    ip_uint_to_str(ip_gateway, state->gw_str);
    ip_uint_to_str(ip_netmask, state->nm_str);
    strcpy(state->dns_str, "8.8.8.8");

    int sb = 94; /* sidebar width */
    /* Display buttons (relative to content area = x + sb) */
    state->btn_brightness_up = ui_button_create(sb+220, 28, 24, 18, "+");
    state->btn_brightness_dn = ui_button_create(sb+248, 28, 24, 18, "-");
    state->btn_theme         = ui_button_create(sb+220, 60, 60, 18, "Toggle");

    /* Network buttons */
    state->btn_driver_rtl    = ui_button_create(sb+60,  28, 70, 18, "RTL8139");
    state->btn_driver_e1k    = ui_button_create(sb+134, 28, 60, 18, "e1000");
    for (int i = 0; i < 4; i++)
        state->btn_net_edit[i] = ui_button_create(sb+310, 58+i*22, 40, 16, "Edit");
    state->btn_net_ok        = ui_button_create(sb+310, 148, 40, 16, "OK");

    /* Sound */
    state->btn_vol_up = ui_button_create(sb+220, 28, 24, 18, "+");
    state->btn_vol_dn = ui_button_create(sb+248, 28, 24, 18, "-");
    state->btn_mute   = ui_button_create(sb+220, 80, 60, 18, "Mute");

    /* System */
    state->btn_mouse_up = ui_button_create(sb+220, 28, 24, 18, "+");
    state->btn_mouse_dn = ui_button_create(sb+248, 28, 24, 18, "-");

    /* Bottom row buttons */
    state->btn_apply = ui_button_create(300, 270, 70, 22, "Apply");
    state->btn_save  = ui_button_create(374, 270, 70, 22, "Save");

    window_t* win = wm_create("Settings", 100, 60, 460, 310,
                              settings_draw, settings_event);
    if (!win) { kfree(state); return -1; }
    win->app_data = state;
    return 0;
}
