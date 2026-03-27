#ifndef WM_H
#define WM_H

#include <stdint.h>
#include "event.h"

#define WM_MAX_WINDOWS  16
#define WM_TITLE_H      24   /* kept for compat — actual chrome uses TITLESTRIP_W */
#define WM_BORDER        1
#define TITLESTRIP_W    20   /* left vertical title strip width */
#define SIDEBAR_W       52   /* launcher rail width */

typedef struct window_s window_t;
typedef void (*wm_draw_fn)(window_t* win, int x, int y, int w, int h);
typedef void (*wm_event_fn)(window_t* win, const event_t* e);

struct window_s {
    int   x, y, w, h;
    char  title[64];
    int   visible;
    int   focused;
    int   dirty;
    void* app_data;
    wm_draw_fn  on_draw;
    wm_event_fn on_event;
};

void      wm_init(void);
window_t* wm_create(const char* title, int x, int y, int w, int h,
                    wm_draw_fn draw, wm_event_fn event);
void wm_invalidate(window_t* win);
void wm_tick(void);
void wm_client_rect(const window_t* win, int* cx, int* cy, int* cw, int* ch);
void wm_focus(window_t* win);
void wm_close(window_t* win);

void wm_launch_terminal(void);
void wm_launch_editor(const char* filename);
void wm_launch_fileman(void);
void wm_launch_settings(void);
void wm_launch_diskmgr(void);
void wm_launch_notepad(void);
void wm_launch_browser(void);
void wm_launch_networkmgr(void);
void system_reboot(void);

#endif
